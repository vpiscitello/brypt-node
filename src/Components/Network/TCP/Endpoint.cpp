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
    std::string_view uri,
    IEndpointMediator const* const pEndpointMediator,
    Network::TCP::Endpoint::SessionTracker const& tracker);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Endpoint(Operation operation)
    : IEndpoint(operation, Protocol::TCP)
    , m_detailsMutex()
    , m_active(false)
    , m_worker()
    , m_context(1)
    , m_acceptor(m_context)
    , m_resolver(m_context)
    , m_eventsMutex()
    , m_events()
    , m_processors()
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

    m_processors = {
        { 
            std::type_index(typeid(BindEvent)),
            [this] (std::any const& event)
            { ProcessBindEvent(std::any_cast<BindEvent const&>(event)); }
        },
        { 
            std::type_index(typeid(ConnectEvent)),
            [this] (std::any const& event)
            { ProcessConnectEvent(std::any_cast<ConnectEvent const&>(event)); }
        },
        { 
            std::type_index(typeid(DispatchEvent)),
            [this] (std::any const& event)
            { ProcessDispatchEvent(std::any_cast<DispatchEvent const&>(event)); }
        },
    };

    m_scheduler = [this] (auto const& destination, auto message) -> bool
    {
        return ScheduleSend(destination, message);
    };
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::~Endpoint()
{
    if (!Shutdown()) {
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
            // [&] (std::stop_tokens) { process_tasks(s); }
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
    // Determine if any of the endpoint's resources are active. If at least one is operating, 
    // then proceed with the shutdown. As a post condition, the operating state should be 
    // be false. 
    bool const operating = m_worker.joinable()  ||   m_acceptor.is_open() ||
        !m_context.stopped() || !m_tracker.IsEmpty();

    if (!operating) { return true; }

    m_spLogger->info("Shutting down endpoint.");

    // Shutdown worker resources. The worker must be stopped first to ensure new data and 
    // sessions are generated as we process the endpoint's shutdown procedure. 
    if (m_active) {
        m_worker.request_stop();
        if (m_worker.joinable()) { m_worker.join(); }
    }
    
    // Shutdown endpoint session resouces. 
    {
        m_acceptor.close(); // Ensure the acceptor has been shutdown.
        m_resolver.cancel(); // Ensure there are no connections being resolved.

        // Stop any active sessions. After stopping the session ensure the context is polled 
        // to ensure the completion handlers have been called. 
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

void Network::TCP::Endpoint::ScheduleBind(BindingAddress const& binding)
{
    if (m_operation != Operation::Server) {
        m_spLogger->critical("Bind was called a non-listening endpoint!");
        throw std::runtime_error("Invalid argument.");
    }

    if (!binding.IsValid()) {
        m_spLogger->error("Unable to parse an endpoint binding for {}!", binding);
        throw std::runtime_error("Invalid argument.");
    }
    assert(Network::Socket::ParseAddressType(binding) != Network::Socket::Type::Invalid);

    auto event = BindEvent(binding);

    // Schedule the Bind network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(event);
    }

    // Greedily set the entry to the provided binding to prevent reflection 
    // connections on startup. If the binding fails or changes, it will be updated by 
    // the thread. 
    m_binding = std::move(binding);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ScheduleConnect(RemoteAddress const& address)
{
    RemoteAddress remote = address;
    ScheduleConnect(std::move(remote));
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ScheduleConnect(RemoteAddress&& address)
{
    ScheduleConnect(std::move(address), nullptr);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ScheduleConnect(
    RemoteAddress&& address, 
    Node::SharedIdentifier const& spIdentifier)
{
    if (m_operation != Operation::Client) {
        m_spLogger->critical("Connect was called on a non-client endpoint!");
        throw std::runtime_error("Invalid argument.");
    }

    assert(address.IsValid() && address.IsBootstrapable());
    assert(Network::Socket::ParseAddressType(address) != Network::Socket::Type::Invalid);

    ConnectEvent event(std::move(address), spIdentifier);

    // Schedule the Connect network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(event);
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleSend(
    Node::Identifier const& identifier, std::string_view message)
{
    // If the message provided is empty, do not send anything
    if (message.empty()) { return false; }

    // Get the socket descriptor of the intended destination. If it is not found, drop the message.
    auto const spSession = m_tracker.Translate(identifier);
    if (!spSession || !spSession->IsActive()) { return false; }

    // Schedule the outgoing message event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(DispatchEvent(spSession, message));
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ServerWorker(std::stop_token token)
{
    // Launch the session acceptor coroutine. 
    boost::asio::co_spawn(
        m_context, ListenProcessor(),
        [this] (std::exception_ptr exception, TCP::CompletionOrigin origin)
        {
            bool const error = (exception || origin == CompletionOrigin::Error);
            if (error) {
                m_spLogger->error("An error caused the listener on {} to shutdown!", m_binding);
                m_active = false;
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
    while(!token.stop_requested()) {
        EventDeque events;
        {
            std::scoped_lock lock(m_eventsMutex);
            for (std::uint32_t idx = 0; idx < Network::Endpoint::EventProcessingLimit; ++idx) {
                // If there are no messages left in the outgoing message queue, break from
                // copying the items into the temporary queue.
                if (m_events.size() == 0) { break; }
                events.emplace_back(m_events.front());
                m_events.pop_front();
            }
        }

        for (auto const& event: events) {
            auto index = std::type_index(event.type());
            if (auto const itr = m_processors.find(index); itr != m_processors.cend()) {
                auto const& [type, processor] = *itr;
                processor(event);
            } else {
                assert(false);
            }
        }

        // If the context has been stopped, restart to ensure the next poll is valid. 
        if (m_context.stopped()) { m_context.restart(); }
        m_context.poll();   // Poll the context for any ready handlers. 
        std::this_thread::sleep_for(Network::Endpoint::CycleTimeout);
    }

    m_active = false; // Indicate that the worker has stopped. 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ProcessBindEvent(BindEvent const& event)
{
    bool success = Bind(event.GetBinding());
    if (!success) {
        m_spLogger->error("A listener on {} could not be established!", event.GetBinding());
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ProcessConnectEvent(ConnectEvent const& event)
{
    auto const status = Connect(event.GetRemoteAddress(), event.GetNodeIdentifier());
    // Connect successes and error logging is handled within the call itself. 
    if (status == ConnectStatusCode::GenericError) {
        m_spLogger->warn("A connection with {} could not be established.",
        event.GetRemoteAddress());
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ProcessDispatchEvent(DispatchEvent const& event)
{
    // If the message provided is empty, do not send anything
    auto const& spSession = event.GetSession();
    auto const& message = event.GetMessage();
    assert(spSession && !message.empty());
    [[maybe_unused]] bool const success = spSession->ScheduleSend(message);
    assert(success);
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Network::TCP::Session> Network::TCP::Endpoint::CreateSession()
{
    auto const spSession = std::make_shared<Network::TCP::Session>(m_context, m_spLogger);

    spSession->OnMessageDispatched(
        [this] (auto const& spSession) { OnMessageDispatched(spSession); });
    spSession->OnMessageReceived(
        [this] (auto const& spSession, auto const& source, auto message) -> bool
        { return OnMessageReceived(spSession, source, message); });
    spSession->OnSessionError(
        [this] (auto const& spSession) { OnSessionStopped(spSession); });

    return spSession;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Bind(BindingAddress const& binding)
{
    auto const OnBindError = [&binding = m_binding, &acceptor = m_acceptor] () -> bool
    {
        binding = BindingAddress();
        acceptor.close();
        return false;
    };

    m_spLogger->info("Opening endpoint on {}.", binding);

    if (m_acceptor.is_open()) { m_acceptor.cancel(); }

    assert(Network::Socket::ParseAddressType(binding) != Network::Socket::Type::Invalid);
    auto const components = Network::Socket::GetAddressComponents(binding);
    boost::system::error_code error;
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string(
            std::string(components.ip.data(), components.ip.size())),
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

    {
        std::scoped_lock lock(m_detailsMutex);
        m_binding = binding;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::ListenProcessor()
{
    boost::system::error_code error;
    while (m_active) {
        if (!m_acceptor.is_open()) { continue; }

        // Create a new session that can be used for the peer that connects. 
        auto const spSession = CreateSession();

        // Await the next connection request. 
        co_await m_acceptor.async_accept(
            spSession->GetSocket(),
            boost::asio::redirect_error(boost::asio::use_awaitable, error));

        // If an error occured accepting the connect, log the error. 
        if (error) {
            // If the error is caused by an intentional operation (i.e. shutdown), then don't 
            // indicate an operational error has occured. 
            if (IsInducedError(error)) { co_return CompletionOrigin::Self; }

            m_spLogger->error(
                "An unexpected error occured while accepting a connection on {}!", m_binding);

            co_return CompletionOrigin::Error;
        }

        OnSessionStarted(spSession);
    }

    co_return CompletionOrigin::Self;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::ConnectStatusCode Network::TCP::Endpoint::Connect(
    RemoteAddress const& address,
    Node::SharedIdentifier const& spIdentifier)
{
    if (!m_pPeerMediator) { return ConnectStatusCode::GenericError; }

    auto const status = local::IsAllowableUri(address.GetUri(), m_pEndpointMediator, m_tracker);
    if (status != ConnectStatusCode::Success) {
        switch (status) {
            case ConnectStatusCode::DuplicateError: {
                m_spLogger->debug("Ignoring duplicate connection attempt to {}.", address);
            } break;
            case ConnectStatusCode::ReflectionError: {
                m_spLogger->debug("Ignoring reflective connection attempt to {}.", address);
            } break;
            default: assert(false); break;
        }
        return status;
    }
    
    boost::asio::co_spawn(
        m_context, ConnectionProcessor(address, spIdentifier),
        [spLogger = m_spLogger, address] (std::exception_ptr exception, TCP::CompletionOrigin origin)
        {
            bool const error = (exception || origin == CompletionOrigin::Error);
            if (error) {
                spLogger->warn("Unable to connect to {} due to an unexpected error.", address);
            }
        });

    return ConnectStatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::ConnectionProcessor(
    RemoteAddress address,
    Node::SharedIdentifier spIdentifier)
{
    boost::system::error_code error;

    assert(Network::Socket::ParseAddressType(address) != Network::Socket::Type::Invalid);
    auto const [ip, port] = Network::Socket::GetAddressComponents(address);
    auto const endpoints = co_await m_resolver.async_resolve(
        ip, port, boost::asio::redirect_error(boost::asio::use_awaitable, error));
    if (error) {
        m_spLogger->warn("Unable to resolve an endpoint for {}", address);
        co_return CompletionOrigin::Error;
    }

    // Get the connection request message from the peer mediator. The mediator will decide 
    // whether or not the address or identifier takes precedence when generating the message. 
    std::optional<std::string> optConnectionRequest = m_pPeerMediator->DeclareResolvingPeer(
        address, spIdentifier);

    // Currently, it is assumed that if we are not provided a connection request it implies
    // that the connection process is on going. Another existing coroutine has been launched
    // and is activley trying to establish a connection 
    if (!optConnectionRequest) { co_return CompletionOrigin::Self; }

    m_spLogger->info("Attempting a connection with {}.", address);

    auto const spSession = CreateSession();

    // Start attempting the connection to the peer. If the connection fails for any reason,
    // the connection processor will wait a period of time until retrying until the number
    // of retries exceeds a predefined limit. 
    std::uint32_t retries = 0;
    do {
        // Launch a connection attempt. 
        co_await boost::asio::async_connect(
            spSession->GetSocket(),
            endpoints,
            boost::asio::redirect_error(boost::asio::use_awaitable, error));
    
        // If an error occurs, log a warning and wait some time be re-attempting. 
        if (error) {
            boost::system::error_code timerError;
            m_spLogger->warn(
                "Unable to connect to {}. Retrying in {} seconds", address,
                Network::Endpoint::ConnectRetryThreshold);
            boost::asio::steady_timer timer(
                m_context, std::chrono::seconds(Network::Endpoint::ConnectRetryThreshold));
            co_await timer.async_wait(
                boost::asio::redirect_error(boost::asio::use_awaitable, timerError));
            if (timerError) { retries = std::numeric_limits<std::uint32_t>::max(); }
        }
    } while (error && ++retries < Network::Endpoint::ConnectRetryThreshold);

    // If a connection could still not be established after some retries, handle cleaning up
    // the connection attempts. 
    if (error) {
        m_pPeerMediator->UndeclareResolvingPeer(address); 
        if (error.value() == boost::asio::error::connection_refused) {
            m_spLogger->warn("Connection refused by {}", address);
            co_return CompletionOrigin::Peer;
        }
        co_return CompletionOrigin::Error;
    }

    OnSessionStarted(spSession);
    
    if (!spSession->ScheduleSend(*optConnectionRequest)) {
        co_return CompletionOrigin::Error;
    }

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
    auto const updater = [this] (ExtendedConnectionDetails& details)
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
    std::shared_ptr<Peer::Proxy> spPeerProxy = {};
    auto const promotedHandler = [&spPeerProxy] (ExtendedConnectionDetails& details)
    {
        spPeerProxy = details.GetPeerProxy();
        details.IncrementMessageSequence();
    };

    auto const unpromotedHandler = [this, &source, &spPeerProxy] (RemoteAddress const& address) 
        -> ExtendedConnectionDetails
    {
        spPeerProxy = IEndpoint::LinkPeer(source, address);
        
        ExtendedConnectionDetails details(spPeerProxy);
        details.SetConnectionState(ConnectionState::Connected);
        details.IncrementMessageSequence();
        
        spPeerProxy->RegisterEndpoint(m_identifier, m_protocol, address, m_scheduler);

        return details;
    };

    // Update the information about the node as it pertains to received data. The node may not be
    // found if this is its first connection.
    m_tracker.UpdateOneConnection(spSession, promotedHandler, unpromotedHandler);

    return spPeerProxy->ScheduleReceive(m_identifier, message);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnMessageDispatched(std::shared_ptr<Session> const& spSession)
{
    auto const updater = [] (ExtendedConnectionDetails& details)
        { details.IncrementMessageSequence(); };

    m_tracker.UpdateOneConnection(spSession, updater);
}

//----------------------------------------------------------------------------------------------------------------------

Network::TCP::ConnectStatusCode local::IsAllowableUri(
    std::string_view uri,
    IEndpointMediator const* const pEndpointMediator,
    Network::TCP::Endpoint::SessionTracker const& tracker)
{
    using namespace Network::TCP;

    // Determine if the provided URI matches any of the node's hosted entrypoints. If the URI 
    // matched an entrypoint, the connection should not be allowed as it would be a connection
    //  to oneself.
    if (pEndpointMediator) {
        auto const entries = pEndpointMediator->GetEndpointEntries();
        for (auto const& [protocol, entry]: entries) {
            if (auto const pos = uri.find(entry); pos != std::string::npos) {
                return ConnectStatusCode::ReflectionError;
            }
        }
    }

    // If the URI matches a currently connected or resolving peer. The connection
    // should not be allowed as it break a valid connection. 
    if (bool const tracked = tracker.IsUriTracked(uri); tracked) {
        return ConnectStatusCode::DuplicateError;
    }

    return ConnectStatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------
