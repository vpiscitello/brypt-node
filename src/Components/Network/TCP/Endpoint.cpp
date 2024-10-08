//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Implementation for the TCP endpoint.
//----------------------------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "EndpointDefinitions.hpp"
#include "SignalService.hpp"
#include "Session.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/EndpointDefinitions.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <spdlog/fmt/chrono.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <coroutine>
#include <map>
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::Endpoint::Agent
{
public:
    explicit Agent(EndpointInstance instance);
    ~Agent();

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool Launched() const;
    void OnEndpointReady();
    
    void Launch();
    void Stop();

    [[nodiscard]] bool Bind(BindingAddress const& binding);
    void Connect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier);

private:
    using AgentInstance = Agent&;
    using TicketNumber = std::size_t;

    void Setup();
    void Teardown();

    struct Listener {
        Listener(AgentInstance instance, boost::asio::io_context& context);
        [[nodiscard]] SocketProcessor operator()();
        [[nodiscard]] bool Bind(BindingAddress const& binding);
        [[nodiscard]] BindingFailure GetFailure(boost::system::error_code const& error) const;

        AgentInstance m_agent;
        boost::asio::ip::tcp::acceptor m_acceptor;
        bool m_rebinding;
        boost::system::error_code m_error;
    };

    struct Delegate {
        using ResolveResult = boost::asio::awaitable<bool>;
        using ConnectResult = boost::asio::awaitable<std::optional<bool>>;

        Delegate(AgentInstance instance, RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier);

        [[nodiscard]] RemoteAddress const& GetAddress() const;
        [[nodiscard]] SocketProcessor operator()();
        [[nodiscard]] ResolveResult Resolve(boost::asio::ip::tcp::resolver::results_type& resolved);
        [[nodiscard]] ConnectResult TryConnect(boost::asio::ip::tcp::resolver::results_type const& resolved);
        [[nodiscard]] CompletionOrigin GetCompletionOrigin() const;
        void Cancel();

        AgentInstance m_agent;
        RemoteAddress m_address;
        Node::SharedIdentifier m_spIdentifier;
        SharedSession m_spSession;
        std::uint32_t m_attempts;
        std::chrono::milliseconds const m_timeout;
        std::uint32_t const m_limit;
        std::chrono::milliseconds const m_interval;
        boost::asio::steady_timer m_watcher;
        boost::system::error_code m_error;
    };

    EndpointInstance m_endpoint;
    std::latch m_latch;
    std::atomic_bool m_active;
    std::jthread m_worker;

    ExclusiveSignalService m_signal;
    Listener m_listener;
    boost::asio::ip::tcp::resolver m_resolver;
    std::map<TicketNumber, Delegate> m_delegates;
};

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Endpoint(Network::Endpoint::Properties const& properties)
    : IEndpoint(properties)
    , m_detailsMutex()
    , m_context(1)
    , m_upAgent()
    , m_tracker()
    , m_messenger()
    , m_disconnector()
    , m_logger(spdlog::get(Logger::Name.data()))
{
    assert(m_properties.GetProtocol() == Protocol::TCP);
    assert(m_logger);

    m_messenger = [this] (Node::Identifier const& destination, MessageVariant&& message) -> bool
        { return ScheduleSend(destination, std::move(message)); };

    m_disconnector = [this] (RemoteAddress const& address) -> bool { return ScheduleDisconnect(address); };
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::~Endpoint()
{
    if (!Shutdown()) [[unlikely]] { m_logger->error("An unexpected error occurred during endpoint shutdown!"); }
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Network::TCP::Endpoint::GetProtocol() const
{
    return ProtocolType;
}

//----------------------------------------------------------------------------------------------------------------------

std::string_view Network::TCP::Endpoint::GetScheme() const
{
    return Scheme;
}

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress Network::TCP::Endpoint::GetBinding() const
{
    std::scoped_lock lock(m_detailsMutex);
    return m_properties.GetBinding();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Startup()
{
    // Create and start a new tcp agent based on the constructed operation type. The Server and Client agents
    // will manage the event loop and resources of this endpoint in a thread. 
    if (m_upAgent && m_upAgent->IsActive()) { return; }
    m_upAgent = std::make_unique<Agent>(*this);
    m_upAgent->OnEndpointReady(); // Notify the agent thread that all resources have been created and work can proceed. 
    assert(m_upAgent && m_upAgent->Launched());
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Shutdown()
{
    // Determine if any of the endpoint's resources are active. If at least one is operating, then proceed with the 
    // shutdown. As a post condition, the operating state should be false. 
    bool const operating = m_upAgent || !m_context.stopped() || !m_tracker.IsEmpty();
    if (!operating) { return true; }

    IEndpoint::OnShutdownRequested();
    m_logger->debug("Shutting down endpoint.");
    
    if (m_upAgent) { m_upAgent.reset(); } // Shutdown the active agent if one has been created. 

    // Stop any active sessions and poll the asio context to ensure the completion handlers have been called. 
    auto const StopSession = [this] (auto const& spSession, auto const&) -> CallbackIteration {
        spSession->Stop();
        PollContext();
        return CallbackIteration::Continue;
    };

    m_tracker.ResetConnections(StopSession); // Reset the tracker's sessions and call stop for each item. 
    m_context.stop(); // Ensure the io context has been stopped. 
    assert(m_tracker.IsEmpty() && m_context.stopped());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::IsActive() const
{
    return (m_upAgent && m_upAgent->IsActive());
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleBind(BindingAddress const& binding)
{
    assert(binding.IsValid());
    assert(Network::Socket::ParseAddressType(binding) != Network::Socket::Type::Invalid);

    auto handler = std::bind(&Endpoint::OnBindEvent, this, BindEvent{ binding });
    boost::asio::post(m_context, std::move(handler));

    // Greedily set the entry to the provided binding to prevent reflection connections on startup. If the binding 
    // fails or changes, it will be updated by the thread. 
    m_properties.SetBinding(binding);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleConnect(RemoteAddress const& address)
{
    return ScheduleConnect(RemoteAddress{ address }, nullptr);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleConnect(RemoteAddress&& address)
{
    return ScheduleConnect(std::move(address), nullptr);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleConnect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier)
{
    assert(address.IsValid() && address.IsBootstrapable());
    assert(Network::Socket::ParseAddressType(address) != Network::Socket::Type::Invalid);

    auto handler = std::bind(&Endpoint::OnConnectEvent, this, ConnectEvent{ std::move(address), spIdentifier });
    boost::asio::post(m_context, std::move(handler));

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleDisconnect(RemoteAddress const& address)
{
    return ScheduleDisconnect(RemoteAddress{ address });
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleDisconnect(RemoteAddress&& address)
{
    assert(address.IsValid() && Network::Socket::ParseAddressType(address) != Network::Socket::Type::Invalid);

    if (!m_tracker.IsUriTracked(address.GetUri())) { return false; }

    auto handler = std::bind(&Endpoint::OnDisconnectEvent, this, DisconnectEvent{ std::move(address) });
    boost::asio::post(m_context, std::move(handler));

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleSend(Node::Identifier const& identifier, std::string&& message)
{
    assert(!message.empty()); // We assert the caller must not send an empty message. 
    return ScheduleSend(identifier, MessageVariant{ std::move(message) });
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleSend(
    Node::Identifier const& identifier, Message::ShareablePack const& spSharedPack)
{
    assert(spSharedPack && !spSharedPack->empty()); // We assert the caller must not send an empty message. 
    return ScheduleSend(identifier, MessageVariant{ spSharedPack });
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleSend(Node::Identifier const& identifier, MessageVariant&& message)
{
    auto const spSession = m_tracker.Translate(identifier);
    // If the associated session can't be found or inactive, drop the message. 
    if (!spSession || !spSession->IsActive()) { return false; }

    auto handler = std::bind(&Endpoint::OnDispatchEvent, this, DispatchEvent{ spSession, std::move(message) });
    boost::asio::post(m_context, std::move(handler));

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::PollContext()
{
    // Note: Context polling should only occur when the main io_context.run() is not active. This method is used to 
    // help ensure completion handlers are called before and after ProcessEvents() has been called. 
    if (m_context.stopped()) { m_context.restart(); } // Ensure the next io_context.poll() call is valid. 
    m_context.poll(); // Poll the context for any ready handlers. 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ProcessEvents(std::stop_token token)
{
    // Keep the event loop running while the thread should be active. Relying on the stopped state of the asio context 
    // is not sufficient as io_context.run() may return any time no work is available. Not having ready handlers does
    // not imply the endpoint has received a requested shutdown. 
    while(!token.stop_requested()) {
        if (m_context.stopped()) { m_context.restart(); } // Ensure the next io_context.run() call is valid. 
        m_context.run(); // Run the asio context until there is no more work to be done. 
        std::this_thread::yield(); // Wait a small period of time before starting the next cycle. 
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnBindEvent(BindEvent const& event)
{
    if (bool const success = m_upAgent->Bind(event.GetBinding()); !success) {
        m_logger->error("A listener on {} could not be established!", event.GetBinding());
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnConnectEvent(ConnectEvent& event)
{
    m_upAgent->Connect(event.ReleaseAddress(), event.GetNodeIdentifier());
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnDisconnectEvent(DisconnectEvent const& event)
{
    auto const spSession = m_tracker.Translate(event.GetAddress().GetUri());
    assert(spSession);
    
    // When the session is stopped the OnSessionStopped callback will untrack the connection. 
    if (spSession) { spSession->Stop(); }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnDispatchEvent(DispatchEvent& event)
{
    assert(event.IsValid());
    auto const& spSession = event.GetSession();
    [[maybe_unused]] bool const success = spSession->ScheduleSend(event.ReleaseMessage());
    assert(success);
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SharedSession Network::TCP::Endpoint::CreateSession()
{
    auto const spSession = std::make_shared<Network::TCP::Session>(m_context, m_logger);

    spSession->Subscribe<Session::Event::Receive>(
        [this] (auto const& spSession, auto const& source, auto message) -> bool {
            return OnMessageReceived(spSession, source, message);
        });

    spSession->Subscribe<Session::Event::Stop>(
        [this] (auto const& spSession) { OnSessionStopped(spSession); });

    return spSession;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnSessionStarted(
    SharedSession const& spSession, RemoteAddress::Origin origin, bool bootstrappable)
{
    spSession->Initialize(origin, bootstrappable); // Initialize the session.
    m_tracker.TrackConnection(spSession, spSession->GetAddress()); // Start tracking the session's for communication.
    spSession->Start(); // Start the session's dispatcher and receiver handlers. 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnSessionStopped(SharedSession const& spSession)
{
    auto const updater = [this, &spSession] (ExtendedDetails& details) { 
        details.SetConnectionState(Connection::State::Disconnected);
        if (auto const spProxy = details.GetPeerProxy(); spProxy) {
            using enum Peer::Proxy::WithdrawalCause;
            auto cause = NetworkShutdown; // Default the endpoint withdrawal reason to indicate it has been requested. 

            // If the endpoint is not shutting down, determine the withdrawal reason from the session's stop cause. 
            if (!IEndpoint::IsStopping()) {
                switch (spSession->GetStopCause()) {
                    case Session::StopCause::Requested: cause = DisconnectRequest; break;
                    case Session::StopCause::Closed: cause = SessionClosure; break;
                    case Session::StopCause::UnexpectedError: cause = UnexpectedError; break;
                    default: break;
                }
            }

            spProxy->WithdrawEndpoint(m_identifier, cause);
        }
    };

    m_tracker.UpdateOneConnection(spSession, updater);

    // When the endpoint is stopping, it is actively resetting all connections. Untracking here would cause the
    // iterators to be invalidated and an use-after-free. 
    if (!IsStopping()) { m_tracker.UntrackConnection(spSession); }
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::OnMessageReceived(
    SharedSession const& spSession, Node::Identifier const& source, std::span<std::uint8_t const> message)
{
    std::shared_ptr<Peer::Proxy> spProxy = {};
    auto const onPromotedProxy = [&spProxy] (ExtendedDetails& details) { spProxy = details.GetPeerProxy(); };
    auto const onUnpromotedProxy = [this, &source, &spProxy] (RemoteAddress const& address) -> ExtendedDetails {
        spProxy = IEndpoint::LinkPeer(source, address);
        
        ExtendedDetails details(spProxy);
        details.SetConnectionState(Connection::State::Connected);
        spProxy->RegisterEndpoint(m_identifier, Protocol::TCP, address, m_messenger, m_disconnector);

        return details;
    };

    // Update the information about the node as it pertains to received data. The node may not be found if this is 
    // its first connection.
    m_tracker.UpdateOneConnection(spSession, onPromotedProxy, onUnpromotedProxy);

    return spProxy->ScheduleReceive(m_identifier, message);
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::ConflictResult Network::TCP::Endpoint::IsConflictingAddress(
    RemoteAddress const& address, Node::SharedIdentifier const& spIdentifier) const
{
    // Determine if the provided URI matches any of the node's hosted entrypoints. If the URI matched an entrypoint, 
    // the connection should not be allowed as it would be a connection to oneself.
    if (m_pEndpointMediator && m_pEndpointMediator->IsRegisteredAddress(address)) {
        return ConflictResult::Reflective;
    }

    // If the URI matches a currently connected or resolving peer. The connection should not be allowed as it break 
    // a valid connection. 
    if (m_tracker.IsKnownPeerOrUri(address.GetUri(), spIdentifier)) { return ConflictResult::Duplicate; }

    return ConflictResult::Success;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Agent::Agent(EndpointInstance instance)
    : m_endpoint(instance)
    , m_latch(2)
    , m_active(false)
    , m_worker()
    , m_signal(instance.m_context.get_executor())
    , m_listener(*this, instance.m_context)
    , m_resolver(instance.m_context)
    , m_delegates()
{
    Launch();
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Agent::IsActive() const
{
    return m_active;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Agent::Launched() const
{
    return m_worker.joinable();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Agent::OnEndpointReady()
{
    m_latch.count_down();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Agent::Launch()
{
    // The core event loop remains unchanged between the server and client. The difference between the two are the 
    // resources created and the processable events. 
    m_worker = std::jthread([this] (std::stop_token token) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Setup(); // Notify the derived agent that it should setup resources for the thread. 
        m_latch.arrive_and_wait(); // Await a ready signal from the spawning thread to ensure resources are accessible. 
        m_active = true; // Indicate that the event processing loop has begun. 
        m_endpoint.OnStarted(); // Trigger the EndpointStarted event after the thread is in a fully ready state. 
        m_endpoint.ProcessEvents(token); // Handle the event loop, this will block until a stop is requested. 
        m_active = false; // Indicate that the event processing loop has ended. 
        Teardown(); // Notify the derived agent that it should teardown resources used is the thread. 
        m_endpoint.PollContext(); // Give any waiting teardown handlers a chance to run. 
        m_endpoint.OnStopped(); // Trigger the EndpointStopped event after the thread is in a fully stopped state. 
    });

    assert(m_worker.joinable()); // A thread must be spawned as a result of Launch(). 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Agent::Stop()
{
    // Note: A derived must call this method during destruction. Otherwise, the teardown method of the thread will 
    // reference a destroyed virtual method. 
    m_worker.request_stop(); // Notify the worker that the endpoint has received a shutdown request. 
    m_endpoint.m_context.stop(); // Stop the asio context such that the thread can check for the shutdown request. 
    if (m_worker.joinable()) { m_worker.join(); }
    assert(!m_active && !m_worker.joinable()); // The event loop and worker thread should not be active after Stop(). 
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Agent::~Agent()
{
    // Ensuring the rebinding flag is false will cause the listener to treat cancellation as a shutdown request. 
    m_listener.m_rebinding = false;
    Agent::Stop();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Agent::Setup()
{
    boost::asio::co_spawn(m_endpoint.m_context, m_listener(), [this] (std::exception_ptr exception, auto origin) {
        constexpr std::string_view ErrorMessage = "An unexpected error caused the listener on {} to shutdown";
        if (bool const error = (exception || origin == CompletionOrigin::Error); error) {
            m_endpoint.m_logger->error(ErrorMessage, m_endpoint.m_properties.GetBinding());
            m_endpoint.OnUnexpectedError();
            m_worker.request_stop();
        }
    });
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Agent::Teardown()
{
    m_signal.cancel();
    m_listener.m_acceptor.close();
    m_resolver.cancel();
    std::ranges::for_each(m_delegates | std::views::values, [] (auto& delegate) { delegate.Cancel(); });
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Agent::Bind(BindingAddress const& binding)
{
    // Note: This method must always be called from the thread managing the lifecycle of the listener's coroutine. 
    // Otherwise, the coroutine could resume while we are updating it's resources. 
    assert(Network::Socket::ParseAddressType(binding) != Network::Socket::Type::Invalid);
    assert(m_endpoint.m_properties.GetBinding() == binding); // Currently, we greedily set the binding in the scheduler. 
    m_endpoint.m_logger->info("Opening listener on {}.", binding);

    bool const success = m_listener.Bind(binding);
    if (success) {    
        m_signal.notify(); // Wake the waiting listener after binding. 
        m_endpoint.OnBindingUpdated(binding);
    }

    return success;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Agent::Listener::Listener(AgentInstance instance, boost::asio::io_context& context)
    : m_agent(instance)
    , m_acceptor(context)
    , m_rebinding(false)
    , m_error()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::Agent::Listener::operator()()
{
    constexpr std::string_view WaitError = "An unexpected error occurred while waiting for a binding!";
    constexpr std::string_view AcceptError = "An unexpected error occurred while accepting a connection on {}!";

    auto& endpoint = m_agent.m_endpoint;
    while (m_agent.m_active) {
        if (!m_acceptor.is_open()) {
            co_await m_agent.m_signal.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, m_error));
            if (m_error) {
                // If the error is caused by an intentional operation (i.e. shutdown), then it is not unexpected. 
                if (IsInducedError(m_error)) { co_return CompletionOrigin::Self; }
                m_agent.m_endpoint.m_logger->error(WaitError);
                m_agent.m_endpoint.OnBindingFailed(m_agent.m_endpoint.m_properties.GetBinding(), GetFailure(m_error));
                co_return CompletionOrigin::Error;
            }
        } else {
            // Create a new session that can be used for the peer that connects. 
            auto const spSession = endpoint.CreateSession();

            // Start the acceptor coroutine, if successful the session's socket will be open on return. 
            co_await m_acceptor.async_accept(
                spSession->GetSocket(), boost::asio::redirect_error(boost::asio::use_awaitable, m_error));
            if (m_error) { 
                // If the error is due to a rebinding operation, drop the session and skip to the next loop iteration. 
                if (m_rebinding) { m_rebinding = false; continue; } 
                // Otherwise, the error should be handled normally (e.g. shutdowns or unexpected errors).
                if (IsInducedError(m_error)) { co_return CompletionOrigin::Self; }
                m_agent.m_endpoint.m_logger->error(AcceptError, endpoint.m_properties.GetBinding());
                m_agent.m_endpoint.OnBindingFailed(endpoint.m_properties.GetBinding(), GetFailure(m_error));
                co_return CompletionOrigin::Error;
            }

            // Notify the endpoint that a new connection has been made. 
            endpoint.OnSessionStarted(spSession, RemoteAddress::Origin::Network, false); 
        }
    }

    co_return CompletionOrigin::Self;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Agent::Listener::Bind(BindingAddress const& binding)
{
    // If the acceptor has already been open, set the rebinding flag and cancel any existing acceptor operations. 
    // The rebinding flag will be used to prevent he listener for treating the cancellation as a shutdown request. 
    if (m_acceptor.is_open()) { m_rebinding = true; m_acceptor.close(); }

    auto const components = Network::Socket::GetAddressComponents(binding);
    boost::system::error_code error;
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string(std::string(components.ip.data(), components.ip.size())),
        components.GetPortNumber());

    auto const OnBindError = [&] () -> bool {
        m_acceptor.close();
        m_rebinding = false;
        m_agent.m_endpoint.OnBindingFailed(binding, GetFailure(error));
        return false;
    };

    m_acceptor.open(endpoint.protocol(), error);
    if (error) [[unlikely]] { return OnBindError(); }

    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::keep_alive(true), error);
    if (error) [[unlikely]] { return OnBindError(); }

#if defined(WIN32)
    using namespace boost::asio::detail;
    m_acceptor.set_option(socket_option::boolean<BOOST_ASIO_OS_DEF(SOL_SOCKET), SO_EXCLUSIVEADDRUSE>(true), error);
    if (error) [[unlikely]] { return OnBindError(); }
#else
    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), error);
    if (error) [[unlikely]] { return OnBindError(); }
#endif

    m_acceptor.bind(endpoint, error);
    if (error) [[unlikely]] { return OnBindError(); }

    m_acceptor.listen(boost::asio::ip::tcp::acceptor::max_connections, error);
    if (error) [[unlikely]] { return OnBindError(); }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::BindingFailure Network::TCP::Endpoint::Agent::Listener::GetFailure(
    boost::system::error_code const& error) const
{
    switch (error.value()) {
        case boost::asio::error::operation_aborted:
        case boost::asio::error::shut_down: return BindingFailure::Canceled;
        case boost::asio::error::address_in_use: return BindingFailure::AddressInUse;
        case boost::asio::error::network_down: return BindingFailure::Offline;
        case boost::asio::error::network_unreachable: return BindingFailure::Unreachable;
        case boost::asio::error::no_permission: return BindingFailure::Permissions;
        default: return BindingFailure::UnexpectedError;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Agent::Connect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier)
{
    constexpr std::string_view DuplicateWarning = "Ignoring duplicate connection attempt to {}.";
    constexpr std::string_view ReflectiveWarning = "Ignoring reflective connection attempt to {}.";
    constexpr auto TicketGenerator = AddressHasher<RemoteAddress>{};
    assert(m_endpoint.m_pResolutionService);

    auto const& logger = m_endpoint.m_logger;
    switch (m_endpoint.IsConflictingAddress(address, spIdentifier)) {
        case ConflictResult::Success: break; // If the address doesn't conflict with any existing address we can proceed. 
        // If an error has occurred, log a debugging statement and early return.
        case ConflictResult::Duplicate: { 
            logger->debug(DuplicateWarning, address);
            m_endpoint.OnConnectionFailed(address, ConnectionFailure::Duplicate);
        } return;
        case ConflictResult::Reflective: {
            logger->debug(ReflectiveWarning, address);
            m_endpoint.OnConnectionFailed(address, ConnectionFailure::Reflective);
        } return;
        default: assert(false); break;
    }

    // Construct a new resolver coroutine element. The ticket number will be generated using the std::hash of the 
    // address. If an element is not emplaced, implying this is a duplicate connection attempt and should return early. 
    auto const [itr, emplaced] = m_delegates.try_emplace(
        TicketGenerator(address), *this, std::move(address), spIdentifier);
    if (!emplaced) { 
        logger->debug(DuplicateWarning, address);
        return;
    }

    // Launch the resolver as a coroutine. Instead of capturing the address by value we capture the ticket number for
    // the completion handler. We the coroutine finishes execution the lifetime of the resolver will be completed. 
    auto& [ticket, delegate] = *itr;  
    boost::asio::co_spawn(m_endpoint.m_context, delegate(), [this, ticket] (std::exception_ptr exception, auto origin) {
        constexpr std::string_view ResolverError = "Unable to connect to {} due to an error.";
        if (bool const error = (exception || origin == CompletionOrigin::Error); error) {
            auto const itr = m_delegates.find(ticket); assert(itr != m_delegates.end());
            auto const& address = itr->second.GetAddress();
            m_endpoint.m_logger->warn(ResolverError, address);
        }
        m_delegates.erase(ticket); // The lifetime of the resolving coroutine is now complete. 
    });
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Agent::Delegate::Delegate(
    AgentInstance instance, RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier)
    : m_agent(instance)
    , m_address(std::move(address))
    , m_spIdentifier(spIdentifier)
    , m_spSession()
    , m_attempts(0)
    , m_timeout(instance.m_endpoint.m_properties.GetConnectionTimeout())
    , m_limit(instance.m_endpoint.m_properties.GetConnectionRetryLimit())
    , m_interval(instance.m_endpoint.m_properties.GetConnectionRetryInterval())
    , m_watcher(instance.m_endpoint.m_context)
    , m_error()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress const& Network::TCP::Endpoint::Agent::Delegate::GetAddress() const
{
    return m_address;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::Agent::Delegate::operator()()
{
    try {
        // Get the connection request message from the peer mediator. The mediator will decide whether or not the address 
        // or identifier takes precedence when generating the message. Currently, it is assumed that if we are not provided
        // a connection request it implies that the connection process is on going. Another existing coroutine has been
        // launched and is actively trying to establish a connection 
        auto const pResolutionService = m_agent.m_endpoint.m_pResolutionService;
        std::optional<std::string> optConnectionRequest = pResolutionService->DeclareResolvingPeer(m_address, m_spIdentifier);
        if (!optConnectionRequest) {
            m_agent.m_endpoint.OnConnectionFailed(m_address, ConnectionFailure::InProgress);
            co_return CompletionOrigin::Self;
        }

        boost::asio::ip::tcp::resolver::results_type resolved;
        if (bool const result = co_await Resolve(resolved); !result) { co_return GetCompletionOrigin(); }

        m_spSession = m_agent.m_endpoint.CreateSession();
        assert(m_spSession);

        m_agent.m_endpoint.m_logger->info("Attempting a connection with {}.", m_address);

        // Start attempting the connection to the peer. If the connection fails for any reason, the connection processor 
        // will wait a period of time until retrying until the number of retries exceeds a predefined limit. 
        std::optional<bool> optConnectStatus;
        do { optConnectStatus = co_await TryConnect(resolved); } while (!optConnectStatus);

        // If a connection could not be established, handle cleaning up the connection attempt. 
        if (optConnectStatus.value() == false) {
            pResolutionService->RescindResolvingPeer(m_address);
            co_return GetCompletionOrigin();
        }

        // The session must be started before sending the initial request. 
        m_agent.m_endpoint.OnSessionStarted(m_spSession, m_address.GetOrigin(), true);

        // Send the initial connection request to the peer. If there is an error indicate the session need
        if (!m_spSession->ScheduleSend(std::move(*optConnectionRequest))) { co_return CompletionOrigin::Error; }

        co_return GetCompletionOrigin();
    } catch(...) {
        m_agent.m_endpoint.OnConnectionFailed(m_address, ConnectionFailure::UnexpectedError);
        co_return CompletionOrigin::Self;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Agent::Delegate::ResolveResult Network::TCP::Endpoint::Agent::Delegate::Resolve(
    boost::asio::ip::tcp::resolver::results_type& resolved)
{
    constexpr std::string_view ResolveWarning = "Unable to resolve {}";
    assert(Network::Socket::ParseAddressType(m_address) != Socket::Type::Invalid);

    auto const [ip, port] = Network::Socket::GetAddressComponents(m_address);
    resolved = co_await m_agent.m_resolver.async_resolve(
        ip, port, boost::asio::redirect_error(boost::asio::use_awaitable, m_error));

    // If an error has occurred, indicate an error and log a warning if not caused by cancellation. 
    if (m_error) {
        if (!IsInducedError(m_error)) { m_agent.m_endpoint.m_logger->warn(ResolveWarning, m_address); }
        co_return false; // Instruct the delegate to halt the connection attempt. 
    }
    
    co_return true; // Indicates that the connection attempt can proceed. 
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Agent::Delegate::ConnectResult Network::TCP::Endpoint::Agent::Delegate::TryConnect(
    boost::asio::ip::tcp::resolver::results_type const& resolved)
{
    constexpr std::string_view RetryWarning= "Unable to connect to {}. Retrying in {}";

    // The connection attempt has been canceled only if the socket error is induced and the endpoint is stopped or 
    // the watcher has not yet timed out. 
    auto const IsCanceled = [this] (boost::system::error_code const& error) -> bool { 
        if (!IsInducedError(error)) { return false; }
        return m_agent.m_endpoint.IsStopping() || m_watcher.expiry() > std::chrono::steady_clock::now();
    };

    // The connection delegate has reached the retry limit it the next attempt exceeds the configured value.
    auto const IsRetryLimitReached = [this] () -> bool { 
        return ++m_attempts > m_limit; 
    };

    m_watcher.expires_after(m_timeout); // Set the timeout for the connection watcher. 

    // Spawn the coroutine that will watch the connection process to detect possible timeouts. 
    boost::asio::co_spawn(m_agent.m_endpoint.m_context,
        [this, wpSession = std::weak_ptr<Session>{ m_spSession }] () -> boost::asio::awaitable<void>{
            boost::system::error_code error;
            // Note: It's possible for the async_connect call to complete before the timer's completion handler 
            // is run (i.e. resources represented by the this pointer could potentially be destroyed). After 
            // continuing from the async_wait call we must use the weak_ptr to ensure the session is still alive. 
            co_await m_watcher.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, error));
            if (!error) {
                if (auto const spSession = wpSession.lock(); spSession && !spSession->IsActive()) { 
                    spSession->GetSocket().cancel();
                }
            }
        }, boost::asio::detached);

    co_await boost::asio::async_connect(
        m_spSession->GetSocket(), resolved, boost::asio::redirect_error(boost::asio::use_awaitable, m_error));
    m_watcher.cancel();

    if (!m_error) { co_return true; } // On success return early such that the client can continue the exchange. 

    if (IsCanceled(m_error)) { co_return false; } // Return early when the operation has been canceled. 
    if (IsRetryLimitReached()) { co_return false; }  // Return early if we have exceeded our available attempts. 

    // If we have reached this point the connection can be scheduled for a future attempt. 
    m_agent.m_endpoint.m_logger->warn(RetryWarning, m_address, m_interval);

    m_error = boost::system::error_code{}; // Enasure the error code has been reset for the timer. 
    boost::asio::steady_timer timer(m_agent.m_endpoint.m_context, m_interval);
    co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, m_error));
    if (m_error) { co_return false; } // All timer errors are currently treated as cancellations. 

    co_return std::nullopt; // Indicates that another connection attempt can be made. 
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::CompletionOrigin Network::TCP::Endpoint::Agent::Delegate::GetCompletionOrigin() const
{
    auto const& logger = m_agent.m_endpoint.m_logger;

    if (!m_error) { return CompletionOrigin::Self; } // If there is no error the connection attempt was successful. 

    auto failure = ConnectionFailure::UnexpectedError;
    auto origin = CompletionOrigin::Error;
    switch (m_error.value()) {
        case boost::asio::error::operation_aborted:
        case boost::asio::error::shut_down: {
            failure = ConnectionFailure::Canceled;
            origin = CompletionOrigin::Self;
        } break;
        case boost::asio::error::connection_refused: {
            logger->warn("Connection refused by {}", m_address);
            failure = ConnectionFailure::Refused;
            origin = CompletionOrigin::Peer;
        } break;
        case boost::asio::error::network_down: failure = ConnectionFailure::Offline; break;
        case boost::asio::error::network_unreachable: failure = ConnectionFailure::Unreachable; break;
        case boost::asio::error::no_permission: failure = ConnectionFailure::Permissions; break;
        default: break;
    }

    m_agent.m_endpoint.OnConnectionFailed(m_address, failure);
    return origin;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Agent::Delegate::Cancel()
{
    // If the resolver has an active session, close the socket as it hasn't been added to the tracker yet. 
    if (m_spSession) { m_spSession->GetSocket().close(); }
}

//----------------------------------------------------------------------------------------------------------------------
