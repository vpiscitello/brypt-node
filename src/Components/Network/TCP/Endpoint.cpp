//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Implementation for the TCP endpoint.
//----------------------------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "EndpointDefinitions.hpp"
#include "Session.hpp"
#include "Components/Network/EndpointDefinitions.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
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

class Network::TCP::Endpoint::Server final : public Network::TCP::Endpoint::Agent
{
public:
    explicit Server(EndpointInstance endpoint);
    ~Server();
    [[nodiscard]] virtual Operation Type() const override;
    [[nodiscard]] bool Bind(BindingAddress const& binding);

private:
    using ServerInstance = Server&;
    virtual void Setup() override;
    virtual void Teardown() override;

    struct Listener {
        Listener(ServerInstance server, boost::asio::io_context& context);
        [[nodiscard]] SocketProcessor operator()();
        [[nodiscard]] bool Bind(BindingAddress const& binding);

        ServerInstance m_server;
        boost::asio::ip::tcp::acceptor m_acceptor;
        boost::system::error_code m_error;
    };

    boost::asio::steady_timer m_signal;
    Listener m_listener;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::TCP::Endpoint::Client final : public Network::TCP::Endpoint::Agent
{
public:
    explicit Client(EndpointInstance instance);
    ~Client();

    [[nodiscard]] virtual Operation Type() const override;
    void Connect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier);

private:
    using ClientInstance = Client&;
    using TicketNumber = std::size_t;

    virtual void Setup() override;
    virtual void Teardown() override;

    struct Delegate {
        enum class Status : std::uint32_t { Indeterminate, Success, Canceled, Refused, UnexpectedError };
        using ResolveResult = boost::asio::awaitable<bool>;
        using ConnectResult = boost::asio::awaitable<void>;

        Delegate(ClientInstance client, RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier);
        [[nodiscard]] RemoteAddress const& GetAddress() const;

        [[nodiscard]] SocketProcessor operator()();
        [[nodiscard]] ResolveResult Resolve(boost::asio::ip::tcp::resolver::results_type& resolved);
        [[nodiscard]] ConnectResult TryConnect(boost::asio::ip::tcp::resolver::results_type const& resolved);
        [[nodiscard]] CompletionOrigin GetCompletionOrigin() const;
        void Cancel();

        ClientInstance m_client;
        RemoteAddress m_address;
        Node::SharedIdentifier m_spIdentifier;
        SharedSession m_spSession;
        std::uint32_t m_attempts;
        Status m_status;
    };

    boost::asio::ip::tcp::resolver m_resolver;
    std::map<TicketNumber, Delegate> m_delegates;
};

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Endpoint(Operation operation, std::shared_ptr<::Event::Publisher> const& spEventPublisher)
    : IEndpoint(Protocol::TCP, operation, spEventPublisher)
    , m_times()
    , m_detailsMutex()
    , m_context(1)
    , m_upAgent()
    , m_eventsMutex()
    , m_events()
    , m_handlers()
    , m_tracker()
    , m_scheduler()
    , m_spLogger()
{
    switch (m_operation) {
        case Operation::Client: m_spLogger = spdlog::get(LogUtils::Name::TcpClient.data()); break;
        case Operation::Server: m_spLogger = spdlog::get(LogUtils::Name::TcpServer.data()); break;
        default: assert(false); break;
    }
    assert(m_spLogger);

    m_handlers = {
        { 
            std::type_index(typeid(BindEvent)),
            [this] (std::any& event) { OnBindEvent(std::any_cast<BindEvent&>(event)); }
        },
        { 
            std::type_index(typeid(ConnectEvent)),
            [this] (std::any& event) { OnConnectEvent(std::any_cast<ConnectEvent&>(event)); }
        },
        { 
            std::type_index(typeid(DispatchEvent)),
            [this] (std::any& event) { OnDispatchEvent(std::any_cast<DispatchEvent&>(event)); }
        },
    };

    m_scheduler = [this] (Node::Identifier const& destination, MessageVariant&& message) -> bool
        { return ScheduleSend(destination, std::move(message)); };
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::~Endpoint()
{
    if (!Shutdown()) [[unlikely]] { m_spLogger->error("An unexpected error occured during endpoint shutdown!"); }
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Network::TCP::Endpoint::GetProtocol() const
{
    return ProtocolType;
}

//----------------------------------------------------------------------------------------------------------------------

std::string Network::TCP::Endpoint::GetScheme() const
{
    return Scheme.data();
}

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress Network::TCP::Endpoint::GetBinding() const
{
    std::scoped_lock lock(m_detailsMutex);
    return m_binding;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Startup()
{
    if (m_upAgent && m_upAgent->IsActive()) { return; } // Only one agent may exist at a time. 

    // Create and start a new tcp agent based on the constructred operation type. The Server and Client agents
    // will manage the event loop and resources of this endpoint in a thread. 
    switch (m_operation) {
        case Operation::Server: m_upAgent = std::make_unique<Server>(*this); break;
        case Operation::Client: m_upAgent = std::make_unique<Client>(*this); break;
        default: return;
    }

    assert(m_upAgent && m_upAgent->Launched());
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Shutdown()
{
    // Determine if any of the endpoint's resources are active. If at least one is operating, then proceed with the 
    // shutdown. As a post condition, the operating state should be false. 
    bool const operating = m_upAgent || !m_context.stopped() || !m_tracker.IsEmpty();
    if (!operating) { return true; }

    m_spLogger->info("Shutting down endpoint.");
    
    // Shutdown the active agent if one has been created. 
    if (m_upAgent) { m_upAgent.reset(); }

    // Stop any active sessions and poll the asio context to ensure the completion handlers have been called. 
    auto const StopSession = [this] (auto const& spSession, auto const&) -> CallbackIteration
    {
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
    assert(m_operation == Operation::Server);
    assert(binding.IsValid());
    assert(Network::Socket::ParseAddressType(binding) != Network::Socket::Type::Invalid);

    BindEvent event(binding);
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(std::move(event));
    }

    // Greedily set the entry to the provided binding to prevent reflection connections on startup. If the binding 
    // fails or changes, it will be updated by the thread. 
    m_binding = binding;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleConnect(RemoteAddress const& address)
{
    RemoteAddress remote = address;
    return ScheduleConnect(std::move(remote));
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleConnect(RemoteAddress&& address)
{
    return ScheduleConnect(std::move(address), nullptr);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleConnect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier)
{
    assert(m_operation == Operation::Client);
    assert(address.IsValid() && address.IsBootstrapable());
    assert(Network::Socket::ParseAddressType(address) != Network::Socket::Type::Invalid);

    ConnectEvent event(std::move(address), spIdentifier);

    // Schedule the Connect network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(event);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleSend(Node::Identifier const& identifier, std::string&& message)
{
    assert(!message.empty()); // We assert the caller must not send an empty message. 
    return ScheduleSend(identifier, MessageVariant{std::move(message)});
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleSend(
    Node::Identifier const& identifier, Message::ShareablePack const& spSharedPack)
{
    assert(spSharedPack && !spSharedPack->empty()); // We assert the caller must not send an empty message. 
    return ScheduleSend(identifier, MessageVariant{spSharedPack});
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleSend(Node::Identifier const& identifier, MessageVariant&& message)
{
    auto const spSession = m_tracker.Translate(identifier);
    // If the associated session can't be found or inactive, drop the message. 
    if (!spSession || !spSession->IsActive()) { return false; }

    // Schedule the outgoing message event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(DispatchEvent(spSession, std::move(message)));
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::PollContext()
{
    // If the context has been stopped, restart to ensure the next poll is valid. 
    if (m_context.stopped()) { m_context.restart(); }
    m_context.poll(); // Poll the context for any ready handlers. 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ProcessEvents(std::stop_token token)
{
    IEndpoint::OnStarted();
    while(!token.stop_requested()) {
        EventDeque events;
        {
            std::scoped_lock lock(m_eventsMutex);
            for (std::uint32_t idx = 0; idx < Network::Endpoint::EventLimit; ++idx) {
                // If there are no messages left in the outgoing message queue, break from copying the items into the
                // temporary queue.
                if (m_events.size() == 0) { break; }
                events.emplace_back(m_events.front());
                m_events.pop_front();
            }
        }

        for (auto& event: events) {
            if (auto const itr = m_handlers.find(std::type_index(event.type())); itr != m_handlers.cend()) {
                auto const& [type, processor] = *itr;
                processor(event);
            } else { assert(false); }
        }

        PollContext();
        std::this_thread::sleep_for(Network::Endpoint::CycleTimeout);
    }

    IEndpoint::OnStopped();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnBindEvent(BindEvent const& event)
{
    assert(m_upAgent && m_upAgent->Type() == Operation::Server);
    auto const pServerAgent = static_cast<Server*>(m_upAgent.get());
    bool success = pServerAgent->Bind(event.GetBinding());
    if (!success) {
        m_spLogger->error("A listener on {} could not be established!", event.GetBinding());
        IEndpoint::OnBindFailed(event.GetBinding());
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnConnectEvent(ConnectEvent& event)
{
    assert(m_upAgent && m_upAgent->Type() == Operation::Client);
    auto const pClientAgent = static_cast<Client*>(m_upAgent.get());
    pClientAgent->Connect(event.ReleaseAddress(), event.GetNodeIdentifier());}

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
    auto const spSession = std::make_shared<Network::TCP::Session>(m_context, m_spLogger);

    spSession->Subscribe<Session::Event::Receive>(
        [this] (auto const& spSession, auto const& source, auto message) -> bool
        {
            return OnMessageReceived(spSession, source, message);
        });

    spSession->Subscribe<Session::Event::Stop>(
        [this] (auto const& spSession, Session::StopCause cause)
        {
            OnSessionStopped(spSession);
            if (cause != Session::StopCause::Requested) { m_tracker.UntrackConnection(spSession); }
        });

    return spSession;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnSessionStarted(SharedSession const& spSession)
{
    // Initialize the session.
    spSession->Initialize(m_operation);

    // Start tracking the descriptor for communication.
    m_tracker.TrackConnection(spSession, spSession->GetAddress());

    // Start the session handlers. 
    spSession->Start();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnSessionStopped(SharedSession const& spSession)
{
    auto const updater = [this] (ExtendedDetails& details)
    { 
        details.SetConnectionState(ConnectionState::Disconnected);
        if (auto const spPeerProxy = details.GetPeerProxy(); spPeerProxy) {
            spPeerProxy->WithdrawEndpoint(m_identifier, m_protocol);
        }
    };

    m_tracker.UpdateOneConnection(spSession, updater);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::OnMessageReceived(
    SharedSession const& spSession, Node::Identifier const& source, std::span<std::uint8_t const> message)
{
    std::shared_ptr<Peer::Proxy> spProxy = {};
    auto const promotedHandler = [&spProxy] (ExtendedDetails& details)
    {
        spProxy = details.GetPeerProxy();
    };

    auto const unpromotedHandler = [this, &source, &spProxy] (RemoteAddress const& address) -> ExtendedDetails
    {
        spProxy = IEndpoint::LinkPeer(source, address);
        
        ExtendedDetails details(spProxy);
        details.SetConnectionState(ConnectionState::Connected);
        spProxy->RegisterEndpoint(m_identifier, m_protocol, address, m_scheduler);

        return details;
    };

    // Update the information about the node as it pertains to received data. The node may not be found if this is 
    // its first connection.
    m_tracker.UpdateOneConnection(spSession, promotedHandler, unpromotedHandler);

    return spProxy->ScheduleReceive(m_identifier, message);
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::ConnectStatus Network::TCP::Endpoint::IsConflictingAddress(RemoteAddress const& address) const
{
    // Determine if the provided URI matches any of the node's hosted entrypoints. If the URI matched an entrypoint, 
    // the connection should not be allowed as it would be a connection to oneself.
    if (m_pEndpointMediator && m_pEndpointMediator->IsAddressRegistered(address)) {
        return ConnectStatus::ReflectionError;
    }

    // If the URI matches a currently connected or resolving peer. The connection should not be allowed as it break 
    // a valid connection. 
    if (m_tracker.IsUriTracked(address.GetUri())) { return ConnectStatus::DuplicateError; }

    return ConnectStatus::Success;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Agent::Agent(EndpointInstance endpoint)
    : m_endpoint(endpoint)
    , m_active(false)
    , m_worker()
{
    // The thread's creation is deferred until Launch() is called. This is to prevent the base classfrom accessing
    // the derived's unitialized members. Derived classes must call Launch() in its constructor body. 
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

void Network::TCP::Endpoint::Agent::Launch(
    std::function<void()> const& setup, std::function<void()> const& teardown)
{
    // The core event loop remains unchanged between the server and client. The difference between the two are the 
    // resources created and the processable events. 
    m_worker = std::jthread([this, setup, teardown] (std::stop_token token)
        {
            setup();
            m_active = true;
            m_endpoint.ProcessEvents(token);
            m_active = false;
            teardown();
        });

    assert(m_worker.joinable());
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Agent::Stop()
{
    // Note: A derived must call this method during destruction. Otherwise, the teardown method of the thread will 
    // reference a destoryed virtual method. 
    m_worker.request_stop();
    if (m_worker.joinable()) { m_worker.join(); }
    assert(!m_active && !m_worker.joinable());
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Server::Server(EndpointInstance endpoint)
    : Agent(endpoint)
    , m_signal(endpoint.m_context)
    , m_listener(*this, endpoint.m_context)
{
    m_signal.expires_at(std::chrono::steady_clock::time_point::max());
    Agent::Launch(std::bind(&Server::Setup, this), std::bind(&Server::Teardown, this));
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Server::~Server()
{
    Agent::Stop();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Operation Network::TCP::Endpoint::Server::Type() const
{
    return Operation::Server;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Server::Setup()
{
    boost::asio::co_spawn(m_endpoint.m_context, m_listener(),
        [this] (std::exception_ptr exception, TCP::CompletionOrigin origin)
        {
            constexpr std::string_view ErrorMessage = "An unexpected error caused the listener on {} to shutdown";
            if (bool const error = (exception || origin == CompletionOrigin::Error); error) {
                m_endpoint.m_spLogger->error(ErrorMessage, m_endpoint.m_binding);
                m_endpoint.OnUnexpectedError();
                m_worker.request_stop();
            }
        });
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Server::Teardown()
{
    m_signal.cancel();
    m_listener.m_acceptor.close(); 
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Server::Bind(BindingAddress const& binding)
{
    // Note: This method must always be called from the thread managing the lifecycle of the listener's coroutine. 
    // Otherwise, the coroutine could resume while we are updating it's resources. 
    assert(Network::Socket::ParseAddressType(binding) != Network::Socket::Type::Invalid);
    assert(m_endpoint.m_binding == binding); // Currently, we greedily set the binding in the scheduler. 
    m_endpoint.m_spLogger->info("Opening endpoint on {}.", binding);

    if (!m_listener.Bind(binding)) { return false; }
    m_signal.cancel_one(); // Wake the awaiting listener coroutine after the acceptor has been bound. 

    m_endpoint.OnBindingUpdated(binding);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Server::Listener::Listener(ServerInstance server, boost::asio::io_context& context)
    : m_server(server)
    , m_acceptor(context)
    , m_error()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::Server::Listener::operator()()
{
    auto& endpoint = m_server.m_endpoint;
    while (m_server.m_active) {
        if (!m_acceptor.is_open()) {
            co_await m_server.m_signal.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, m_error));
        } else {
            // Create a new session that can be used for the peer that connects. 
            auto const spSession = endpoint.CreateSession();

            // Await the next connection request. 
            co_await m_acceptor.async_accept(
                spSession->GetSocket(), boost::asio::redirect_error(boost::asio::use_awaitable, m_error));

            // If an error occured accepting the connect, log the error. 
            if (m_error) {
                // If the error is caused by an intentional operation (i.e. shutdown), then it is not unexpected. 
                if (IsInducedError(m_error)) { co_return CompletionOrigin::Self; }
                endpoint.m_spLogger->error(
                    "An unexpected error occured while accepting a connection on {}!", endpoint.m_binding);
                co_return CompletionOrigin::Error;
            }

            endpoint.OnSessionStarted(spSession);
        }
    }

    co_return CompletionOrigin::Self;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Server::Listener::Bind(BindingAddress const& binding)
{
    auto const OnBindError = [&acceptor = m_acceptor] () -> bool { acceptor.close(); return false; };

    if (m_acceptor.is_open()) { m_acceptor.cancel(); }

    auto const components = Network::Socket::GetAddressComponents(binding);
    boost::system::error_code error;
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string(std::string(components.ip.data(), components.ip.size())),
        components.GetPortNumber());

    m_acceptor.open(endpoint.protocol(), error);
    if (error) [[unlikely]] { return OnBindError(); }

    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::keep_alive(true), error);
    if (error) [[unlikely]] { return OnBindError(); }

    m_acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), error);
    if (error) [[unlikely]] { return OnBindError(); }

    m_acceptor.bind(endpoint, error);
    if (error) [[unlikely]] { return OnBindError(); }

    m_acceptor.listen(boost::asio::ip::tcp::acceptor::max_connections, error);
    if (error) [[unlikely]] { return OnBindError(); }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Client::Client(EndpointInstance endpoint)
    : Agent(endpoint)
    , m_resolver(m_endpoint.m_context)
{
    Agent::Launch(std::bind(&Client::Setup, this), std::bind(&Client::Teardown, this));
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Client::~Client()
{
    Agent::Stop();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Operation Network::TCP::Endpoint::Client::Type() const
{
    return Operation::Client;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Client::Setup()
{
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Client::Teardown()
{
    m_resolver.cancel();
    std::ranges::for_each(m_delegates | std::views::values, [] (auto& delegate) { delegate.Cancel(); });
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Client::Connect(RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier)
{
    constexpr std::string_view DuplicateWarning = "Ignoring duplicate connection attempt to {}.";
    constexpr std::string_view ReflectiveWarning = "Ignoring reflective connection attempt to {}.";
    constexpr auto TicketGenerator = AddressHasher<RemoteAddress>{};
    assert(m_endpoint.m_pPeerMediator);

    auto const& spLogger = m_endpoint.m_spLogger;
    {
        using enum ConnectStatus;
        switch (m_endpoint.IsConflictingAddress(address)) {
            case Success: break; // If the address doesn't conflict with any existing address we can proceed. 
            // If an error has occured, log a debugging statement and early return.
            case DuplicateError: { spLogger->debug(DuplicateWarning, address); } return;
            case ReflectionError: { spLogger->debug(ReflectiveWarning, address); } return;
            case RetryError: assert(false); break; // We should be given Retry error code from this check. 
        }
    }

    // Construct a new resolver coroutine element. The ticket number will be generated using the std::hash of the 
    // address. If an element is not emplaced, implying this is a duplicate connection attempt and should return early. 
    auto const [itr, emplaced] = m_delegates.try_emplace(
        TicketGenerator(address), *this, std::move(address), spIdentifier);
    if (!emplaced) { spLogger->debug(DuplicateWarning, address); return; }
    auto& [ticket, delegate] = *itr;  

    // Launch the resolver as a coroutine. Instead of capturing the address by value we capture the ticket number for
    // the completion handler. We the coroutine finishes execution the lifetime of the resolver will be completed. 
    boost::asio::co_spawn(
        m_endpoint.m_context, delegate(),
        [this, ticket] (std::exception_ptr exception, TCP::CompletionOrigin origin)
        {
            constexpr std::string_view ResolverError = "Unable to connect to {} due to an unexpected error.";
            if (bool const error = (exception || origin == CompletionOrigin::Error); error) {
                auto const itr = m_delegates.find(ticket); assert(itr != m_delegates.end());
                auto const& address = itr->second.GetAddress();
                m_endpoint.m_spLogger->warn(ResolverError, address);
                m_endpoint.OnConnectFailed(address);
            }
            m_delegates.erase(ticket); // The lifetime of the resolving coroutine is now complete. 
        });
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Client::Delegate::Delegate(
    ClientInstance client, RemoteAddress&& address, Node::SharedIdentifier const& spIdentifier)
    : m_client(client)
    , m_address(std::move(address))
    , m_spIdentifier(spIdentifier)
    , m_spSession()
    , m_attempts(0)
    , m_status(Status::Indeterminate)
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress const& Network::TCP::Endpoint::Client::Delegate::GetAddress() const
{
    return m_address;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::Client::Delegate::operator()()
{
    // Get the connection request message from the peer mediator. The mediator will decide whether or not the address 
    // or identifier takes precedence when generating the message. Currently, it is assumed that if we are not provided
    // a connection request it implies that the connection process is on going. Another existing coroutine has been
    // launched and is activley trying to establish a connection 
    auto const pPeerMediator = m_client.m_endpoint.m_pPeerMediator;
    std::optional<std::string> optConnectionRequest = pPeerMediator->DeclareResolvingPeer(m_address, m_spIdentifier);
    if (!optConnectionRequest) { co_return CompletionOrigin::Self; }

    boost::asio::ip::tcp::resolver::results_type resolved;
    if (bool const result = co_await Resolve(resolved); !result) { co_return GetCompletionOrigin(); }

    m_spSession = m_client.m_endpoint.CreateSession();
    assert(m_spSession);

    m_client.m_endpoint.m_spLogger->info("Attempting a connection with {}.", m_address);

    // Start attempting the connection to the peer. If the connection fails for any reason, the connection processor 
    // will wait a period of time until retrying until the number of retries exceeds a predefined limit. 
    do { co_await TryConnect(resolved); } while (m_status == Status::Indeterminate);
    if (m_status != Status::Success) {
         // If a connection could not be established, handle cleaning up the connection attempt. 
        pPeerMediator->RescindResolvingPeer(m_address); 
        co_return GetCompletionOrigin();
    }

    m_client.m_endpoint.OnSessionStarted(m_spSession); // The session must be started before sending the initial request. 

    // Send the initial connection request to the peer. If there is an error indicate the session need
    if (!m_spSession->ScheduleSend(std::move(*optConnectionRequest))) { co_return CompletionOrigin::Error; }

    co_return GetCompletionOrigin();
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Client::Delegate::ResolveResult Network::TCP::Endpoint::Client::Delegate::Resolve(
    boost::asio::ip::tcp::resolver::results_type& resolved)
{
    constexpr std::string_view ResolveWarning = "Unable to resolve an endpoint for {}";
    assert(Network::Socket::ParseAddressType(m_address) != Socket::Type::Invalid);

    boost::system::error_code error;
    auto const [ip, port] = Network::Socket::GetAddressComponents(m_address);
    resolved = co_await m_client.m_resolver.async_resolve(
        ip, port, boost::asio::redirect_error(boost::asio::use_awaitable, error));
    if (error) {
        // If the operation was canceled due to the endpoint shutting down, indicate the resolver should return early.
        // Otherwise, log a warning and indicate an unexpected error. 
        if (IsInducedError(error)) {
            m_status = Status::Canceled;
        } else {
            m_client.m_endpoint.m_spLogger->warn(ResolveWarning, m_address);
            m_status = Status::UnexpectedError;
        }
        co_return false;
    }
    
    co_return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Client::Delegate::ConnectResult Network::TCP::Endpoint::Client::Delegate::TryConnect(
    boost::asio::ip::tcp::resolver::results_type const& resolved)
{
    constexpr std::string_view RetryWarning= "Unable to connect to {}. Retrying in {} seconds";

    ++m_attempts; // Increment the number of attempts made to establish a connection. 
    boost::system::error_code error;
    co_await boost::asio::async_connect(
        m_spSession->GetSocket(), resolved, boost::asio::redirect_error(boost::asio::use_awaitable, error));

    // If the connection succeeded, indicate a success such that the resolver can continue to the next process step. 
    if (!error) { m_status = Status::Success; co_return; }

    // If we have reached the maximum allowed connection attempts, indicate if the reason was caused by the peer or not. 
    if (m_attempts > Network::Endpoint::RetryLimit) {
        m_status = (error == boost::asio::error::connection_refused) ? Status::Refused : Status::UnexpectedError;
        co_return;
    }

    // If the operation was canceled due to the endpoint shutting down, indicate the resolver should return early.
    if (IsInducedError(error)) { m_status = Status::Canceled; co_return; }

    // If no other situation is applicable, handle the error by "scheduling" an attept in the future.
    m_client.m_endpoint.m_spLogger->warn(RetryWarning, m_address, Network::Endpoint::RetryTimeout.count());
        
    boost::system::error_code timerError;
    boost::asio::steady_timer timer(
        m_client.m_endpoint.m_context, std::chrono::seconds(Network::Endpoint::RetryTimeout));
    co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, timerError));
    if (timerError) { m_status = Status::Canceled; } // All timer errors are currently treated as cancellations. 

    co_return;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::CompletionOrigin Network::TCP::Endpoint::Client::Delegate::GetCompletionOrigin() const
{
    using enum CompletionOrigin;
    auto const& spLogger = m_client.m_endpoint.m_spLogger;
    switch (m_status) {
        // Completions caused intentionally, meaning an non-error state. 
        case Status::Success: 
        case Status::Canceled: return Self;
        // Completions caused by the peer (e.g. an offline peer).
        case Status::Refused: { spLogger->warn("Connection refused by {}", m_address); } return Peer;
        // Completions caused by an error state. 
        case Status::UnexpectedError: return Error;
        default: assert(false); return Error;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::Client::Delegate::Cancel()
{
    // If the resolver has an active session, close the socket as it hasn't been added to the tracker yet. 
    if (m_spSession) { m_spSession->GetSocket().close(); }
}

//----------------------------------------------------------------------------------------------------------------------
