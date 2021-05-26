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
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] Network::TCP::ConnectStatusCode IsAllowableUri(
    Network::RemoteAddress const& uri,
    IEndpointMediator const* const pEndpointMediator,
    Network::TCP::Endpoint::SessionTracker const& tracker);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Endpoint(Operation operation, std::shared_ptr<::Event::Publisher> const& spEventPublisher)
    : IEndpoint(Protocol::TCP, operation, spEventPublisher)
    , m_detailsMutex()
    , m_active(false)
    , m_worker()
    , m_context(1)
    , m_acceptor(m_context)
    , m_resolver(m_context)
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
    {
        return ScheduleSend(destination, std::move(message));
    };
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::~Endpoint()
{
    if (!Shutdown()) [[unlikely]] {
        m_spLogger->error("An unexpected error occured during endpoint shutdown!");
    }
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
    // A new worker thread may not be started while there is another currently active. 
    if (m_active) { return; }

    m_active = true; // Indicate that the endpoint has begun spinning up.

    // Attempt to start an endpoint worker thread based on the designated endpoint operation.
    switch (m_operation) {
        case Operation::Server: { 
            m_worker = std::jthread([&] (std::stop_token token) { ServerWorker(token); });
        } break;
        case Operation::Client: {
            m_worker = std::jthread([&] (std::stop_token token) { ClientWorker(token); }); 
        } break;
        default: return;
    }

    assert(m_worker.joinable());
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Shutdown()
{
    // Determine if any of the endpoint's resources are active. If at least one is operating, then proceed with the 
    // shutdown. As a post condition, the operating state should be false. 
    bool const operating = m_worker.joinable() || m_acceptor.is_open() || !m_context.stopped() || !m_tracker.IsEmpty();
    if (!operating) { return true; }

    m_spLogger->info("Shutting down endpoint.");

    // Shutdown worker resources. The worker must be stopped first to ensure new data and sessions are generated as we 
    // process the endpoint's shutdown procedure. 
    if (m_active) {
        m_worker.request_stop();
        if (m_worker.joinable()) { m_worker.join(); }
    }
    
    // Shutdown endpoint session resouces. 
    {
        m_acceptor.close(); // Ensure the acceptor has been shutdown.
        m_resolver.cancel(); // Ensure there are no connections being resolved.

        // Stop any active sessions. After stopping the session ensure the context is polled to ensure the completion 
        // handlers have been called. 
        m_tracker.ResetConnections(
            [&](auto const& spSession, auto const&) -> CallbackIteration
            {
                spSession->Stop();

                if (m_context.stopped()) { m_context.restart(); }
                m_context.poll();

                return CallbackIteration::Continue;
            });

        m_context.stop(); // Ensure the io context has been stopped. 

        assert(m_tracker.IsEmpty() && m_context.stopped());
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::IsActive() const
{
    return m_active;
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

void Network::TCP::Endpoint::ServerWorker(std::stop_token token)
{
    // Launch the session acceptor coroutine. 
    boost::asio::co_spawn(
        m_context, Listener(),
        [this] (std::exception_ptr exception, TCP::CompletionOrigin origin)
        {
            bool const error = (exception || origin == CompletionOrigin::Error);
            if (error) {
                m_spLogger->error("An unexpected error caused the listener on {} to shutdown!", m_binding);
                m_active = false;
                IEndpoint::OnUnexpectedError();
            }
        });

    ProcessEvents(token); // Start the endpoint's event loop 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ClientWorker(std::stop_token  token)
{
    ProcessEvents(token); // Start the endpoint's event loop 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ProcessEvents(std::stop_token token)
{
    IEndpoint::OnStarted();
    while(!token.stop_requested()) {
        EventDeque events;
        {
            std::scoped_lock lock(m_eventsMutex);
            for (std::uint32_t idx = 0; idx < Network::Endpoint::EventProcessingLimit; ++idx) {
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

        // If the context has been stopped, restart to ensure the next poll is valid. 
        if (m_context.stopped()) { m_context.restart(); }
        m_context.poll(); // Poll the context for any ready handlers. 
        std::this_thread::sleep_for(Network::Endpoint::CycleTimeout);
    }

    m_active = false; // Indicate that the worker has stopped. 
    IEndpoint::OnStopped();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnBindEvent(BindEvent const& event)
{
    bool success = Bind(event.GetBinding());
    if (!success) {
        m_spLogger->error("A listener on {} could not be established!", event.GetBinding());
        IEndpoint::OnBindFailed(event.GetBinding());
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnConnectEvent(ConnectEvent const& event)
{
    // Note: the return code does not reflect that the connection was completely successfull. A coroutine is launched
    // to handle final resolution which will handle asynchronous errors as they appear. 
    auto const status = Connect(event.GetRemoteAddress(), event.GetNodeIdentifier());
    // Connect successes and error logging is handled within the call itself. 
    if (status == ConnectStatusCode::GenericError) {
        m_spLogger->warn("A connection with {} could not be established.", event.GetRemoteAddress());
        IEndpoint::OnConnectFailed(event.GetRemoteAddress());
    }
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

std::shared_ptr<Network::TCP::Session> Network::TCP::Endpoint::CreateSession()
{
    auto const spSession = std::make_shared<Network::TCP::Session>(m_context, m_spLogger);

    spSession->OnMessageReceived(
        [this] (auto const& spSession, auto const& source, auto message) -> bool
        { return OnMessageReceived(spSession, source, message); });
    spSession->OnStopped([this] (auto const& spSession) { OnSessionStopped(spSession); });

    return spSession;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Bind(BindingAddress const& binding)
{
    assert(Network::Socket::ParseAddressType(binding) != Network::Socket::Type::Invalid);

    m_spLogger->info("Opening endpoint on {}.", binding);

    auto const OnBindError = [&acceptor = m_acceptor] () -> bool
    {
        acceptor.close();
        return false;
    };

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

    assert(m_binding == binding); // Currently, we greedily set the binding in the scheduler. 

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::Listener()
{
    boost::system::error_code error;
    while (m_active) {
        if (!m_acceptor.is_open()) { continue; }

        // Create a new session that can be used for the peer that connects. 
        auto const spSession = CreateSession();

        // Await the next connection request. 
        co_await m_acceptor.async_accept(
            spSession->GetSocket(), boost::asio::redirect_error(boost::asio::use_awaitable, error));

        // If an error occured accepting the connect, log the error. 
        if (error) {
            // If the error is caused by an intentional operation (i.e. shutdown), then don't indicate an operational
            // error has occured. 
            if (IsInducedError(error)) { co_return CompletionOrigin::Self; }
            m_spLogger->error("An unexpected error occured while accepting a connection on {}!", m_binding);
            co_return CompletionOrigin::Error;
        }

        OnSessionStarted(spSession);
    }

    co_return CompletionOrigin::Self;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::ConnectStatusCode Network::TCP::Endpoint::Connect(
    RemoteAddress const& address, Node::SharedIdentifier const& spIdentifier)
{
    using enum ConnectStatusCode;
    if (!m_pPeerMediator) { return GenericError; }

    if (auto const status = local::IsAllowableUri(address, m_pEndpointMediator, m_tracker); status != Success) {
        switch (status) {
            case DuplicateError: { m_spLogger->debug("Ignoring duplicate connection attempt to {}.", address); } break;
            case ReflectionError: { m_spLogger->debug("Ignoring reflective connection attempt to {}.", address); } break;
            default: assert(false); break;
        }
        return status;
    }
    
    boost::asio::co_spawn(
        m_context, Resolver(address, spIdentifier),
        [this, address] (std::exception_ptr exception, TCP::CompletionOrigin origin)
        {
            bool const error = (exception || origin == CompletionOrigin::Error);
            if (error) {
                m_spLogger->warn("Unable to connect to {} due to an unexpected error.", address);
                IEndpoint::OnConnectFailed(address);
            }
        });

    return Success;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::Resolver(
    RemoteAddress address, Node::SharedIdentifier spIdentifier)
{
    using namespace Network::Endpoint;
    boost::system::error_code error;

    assert(Network::Socket::ParseAddressType(address) != Socket::Type::Invalid);
    auto const [ip, port] = Network::Socket::GetAddressComponents(address);
    auto const endpoints = co_await m_resolver.async_resolve(
        ip, port, boost::asio::redirect_error(boost::asio::use_awaitable, error));
    if (error) {
        m_spLogger->warn("Unable to resolve an endpoint for {}", address);
        co_return CompletionOrigin::Error;
    }

    // Get the connection request message from the peer mediator. The mediator will decide whether or not the address 
    // or identifier takes precedence when generating the message. 
    std::optional<std::string> optConnectionRequest = m_pPeerMediator->DeclareResolvingPeer(address, spIdentifier);

    // Currently, it is assumed that if we are not provided a connection request it implies that the connection process
    //  is on going. Another existing coroutine has been launched and is activley trying to establish a connection 
    if (!optConnectionRequest) { co_return CompletionOrigin::Self; }

    m_spLogger->info("Attempting a connection with {}.", address);

    auto const spSession = CreateSession();

    // Start attempting the connection to the peer. If the connection fails for any reason, the connection processor 
    // will wait a period of time until retrying until the number of retries exceeds a predefined limit. 
    std::uint32_t retries = 0;
    do {
        // Launch a connection attempt. 
        co_await boost::asio::async_connect(
            spSession->GetSocket(), endpoints, boost::asio::redirect_error(boost::asio::use_awaitable, error));
    
        // If an error occurs, log a warning and wait some time be re-attempting. 
        if (error) {
            m_spLogger->warn("Unable to connect to {}. Retrying in {} seconds", address, ConnectRetryThreshold);
                
            boost::system::error_code timerError;
            boost::asio::steady_timer timer(m_context, std::chrono::seconds(ConnectRetryThreshold));
            co_await timer.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, timerError));

            if (timerError) { retries = std::numeric_limits<std::uint32_t>::max(); }
        }
    } while (error && ++retries < ConnectRetryThreshold);

    // If a connection could still not be established after some retries, handle cleaning up the connection attempts. 
    if (error) {
        m_pPeerMediator->UndeclareResolvingPeer(address); 
        if (error.value() == boost::asio::error::connection_refused) {
            m_spLogger->warn("Connection refused by {}", address);
            co_return CompletionOrigin::Peer;
        }
        co_return CompletionOrigin::Error;
    }

    OnSessionStarted(spSession);
    
    if (!spSession->ScheduleSend(std::move(*optConnectionRequest))) { co_return CompletionOrigin::Error; }

    // Send the initial request to the peer. 
    co_return CompletionOrigin::Self;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnSessionStarted(std::shared_ptr<Session> const& spSession)
{
    // Initialize the session.
    spSession->Initialize(m_operation);

    // Start tracking the descriptor for communication.
    m_tracker.TrackConnection(spSession, spSession->GetAddress());

    // Start the session handlers. 
    spSession->Start();
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnSessionStopped(std::shared_ptr<Session> const& spSession)
{
    auto const updater = [this] (ExtendedDetails& details)
    { 
        details.SetConnectionState(ConnectionState::Disconnected);
        if (auto const spPeerProxy = details.GetPeerProxy(); spPeerProxy) {
            spPeerProxy->WithdrawEndpoint(m_identifier, m_protocol);
        }
    };

    m_tracker.UpdateOneConnection(spSession, updater);

    if (m_active) { m_tracker.UntrackConnection(spSession); }
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::OnMessageReceived(
    std::shared_ptr<Session> const& spSession,
    Node::Identifier const& source,
    std::span<std::uint8_t const> message)
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

Network::TCP::ConnectStatusCode local::IsAllowableUri(
    Network::RemoteAddress const& address,
    IEndpointMediator const* const pEndpointMediator,
    Network::TCP::Endpoint::SessionTracker const& tracker)
{
    using enum Network::TCP::ConnectStatusCode;

    // Determine if the provided URI matches any of the node's hosted entrypoints. If the URI matched an entrypoint, 
    // the connection should not be allowed as it would be a connection to oneself.
    if (pEndpointMediator && pEndpointMediator->IsAddressRegistered(address)) [[likely]] {
        return ReflectionError;
    }

    // If the URI matches a currently connected or resolving peer. The connection should not be allowed as it break 
    // a valid connection. 
    if (tracker.IsUriTracked(address.GetUri())) { return DuplicateError; }

    return Success;
}

//----------------------------------------------------------------------------------------------------------------------
