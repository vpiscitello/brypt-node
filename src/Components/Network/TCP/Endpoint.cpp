//------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Implementation for the TCP endpoint.
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "EndpointDefinitions.hpp"
#include "Session.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/Network/EndpointDefinitions.hpp"
#include "Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/lexical_cast.hpp>
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <coroutine>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

[[nodiscard]] Network::TCP::ConnectStatusCode IsAllowableUri(
    std::string_view uri,
    IEndpointMediator const* const pEndpointMediator,
    Network::TCP::Endpoint::SessionTracker const& tracker);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::Endpoint(std::string_view interface, Operation operation)
    : IEndpoint(interface, operation, Protocol::TCP)
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
{
    m_processors = {
        { 
            std::type_index(typeid(InstructionEvent)),
            [this] (std::any const& event)
            {
                ProcessNetworkInstruction(
                    std::any_cast<InstructionEvent const&>(event));
            }
        },
        { 
            std::type_index(typeid(DispatchEvent)),
            [this] (std::any const& event)
            {
                ProcessOutgoingMessage(
                    std::any_cast<DispatchEvent const&>(event));
            }
        },
    };

    m_scheduler = [this] (auto const& destination, auto message) -> bool
    {
        return ScheduleSend(destination, message);
    };
}

//------------------------------------------------------------------------------------------------

Network::TCP::Endpoint::~Endpoint()
{
    if (!Shutdown()) {
        std::ostringstream notification;
        notification << "[TCP] An unexpected error occured during endpoint shutdown";
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
    }
}

//------------------------------------------------------------------------------------------------

Network::Protocol Network::TCP::Endpoint::GetProtocol() const
{
    return ProtocolType;
}

//------------------------------------------------------------------------------------------------

std::string Network::TCP::Endpoint::GetProtocolString() const
{
    return ProtocolString.data();
}

//------------------------------------------------------------------------------------------------

std::string Network::TCP::Endpoint::GetEntry() const
{
    std::scoped_lock lock(m_detailsMutex);
    return m_entry;
}

//------------------------------------------------------------------------------------------------

std::string Network::TCP::Endpoint::GetURI() const
{
    std::scoped_lock lock(m_detailsMutex);
    return Scheme.data() + m_entry;
}

//------------------------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Shutdown()
{
    {
        NodeUtils::printo("[TCP] Shutting down endpoint", NodeUtils::PrintType::Endpoint);
    }

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

//------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::IsActive() const
{
    return m_active;
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ScheduleBind(std::string_view binding)
{
    if (m_operation != Operation::Server) {
        throw std::runtime_error("Bind was called a non-listening Endpoint!");
    }

    TCP::InstructionEvent event;
    try {
        auto [address, port] = NetworkUtils::SplitAddressString(binding);
        if (address.empty() || port.empty()) { return; }
        event.instruction = Instruction::Bind;
        event.address = std::move(address);
        event.port = boost::lexical_cast<std::uint16_t>(port);
    } catch (...) {
        std::ostringstream notification;
        notification << "[TCP] Error occured attempting to schedule a bind to " << binding << "!";
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
    }

    if (auto const found = event.address.find(NetworkUtils::Wildcard); found != std::string::npos) {
        event.address = NetworkUtils::GetInterfaceAddress(m_interface);
    }

    // Schedule the Bind network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(event);
    }
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ScheduleConnect(std::string_view entry)
{
    if (m_operation != Operation::Client) {
        throw std::runtime_error("Connect was called a non-client Endpoint!");
    }
    
    TCP::InstructionEvent event;
    try {
        auto [address, port] = NetworkUtils::SplitAddressString(entry);
        if (address.empty() || port.empty()) { return; }
        event.instruction = Instruction::Connect;
        event.address = std::move(address);
        event.port = boost::lexical_cast<std::uint16_t>(port);
    } catch (...) {
        std::ostringstream notification;
        notification << "[TCP] Error occured attempting to schedule a connection to " << entry << "!";
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
    }

    // Schedule the Connect network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(event);
    }
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ScheduleConnect(
    BryptIdentifier::SharedContainer const& spIdentifier,
    std::string_view entry)
{
    if (m_operation != Operation::Client) {
        throw std::runtime_error("Connect was called a non-client Endpoint!");
    }

    TCP::InstructionEvent event;
    try {
        auto [address, port] = NetworkUtils::SplitAddressString(entry);
        if (address.empty() || port.empty()) { return; }
        event.identifier = spIdentifier;
        event.instruction = Instruction::Connect;
        event.address = std::move(address);
        event.port = boost::lexical_cast<std::uint16_t>(port);
    } catch (...) {
        std::ostringstream notification;
        notification << "[TCP] Error occured attempting to schedule a connection to " << entry << "!";
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
    }

    // Schedule the Connect network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(event);
    }
}

//------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::ScheduleSend(
    BryptIdentifier::Container const& identifier, std::string_view message)
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

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ServerWorker(std::stop_token token)
{
    // Launch the session acceptor coroutine. 
    boost::asio::co_spawn(
        m_context, Listen(),
        [this] (std::exception_ptr exception, TCP::CompletionOrigin origin)
        {
            bool const error = (exception || origin == TCP::CompletionOrigin::Error);
            if (error) [[unlikely]] {
                std::ostringstream notification;
                notification << "[TCP] An error caused the server on " << m_entry;
                notification  << " to shutdown!";
                NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
                m_active = false;
            }
        });

    ProcessEvents(token); // Start the endpoint's event loop 
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ClientWorker(std::stop_token  token)
{
    ProcessEvents(token); // Start the endpoint's event loop 
}

//------------------------------------------------------------------------------------------------

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
            if (const auto itr = m_processors.find(std::type_index(event.type()));
                itr != m_processors.cend()) {
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

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ProcessNetworkInstruction(
    TCP::InstructionEvent const& event)
{
    bool success = false;
    switch (event.instruction) {
        case Instruction::Bind: {
            success = Bind(event.address, event.port);
        } break;
        case Instruction::Connect: {
            auto const status = Connect(event.address, event.port, event.identifier);
            if (status == ConnectStatusCode::Success) { success = true; }
        } break;
        default: assert(false); break; // What is this?
    }

    if (!success) {
        std::ostringstream notification;
        notification << "[TCP] ";
        switch (event.instruction) {
            case Instruction::Bind: notification << "Bind to "; break;
            case Instruction::Connect: notification << "Connection to "; break;
            default: assert(false); break; // What is this? 
        }
        notification << event.address << NetworkUtils::ComponentSeperator;
        notification << event.port << " failed.";
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
        notification.clear();
    }
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::ProcessOutgoingMessage(DispatchEvent const& event)
{
    // If the message provided is empty, do not send anything
    assert(event.session && !event.message.empty());
    [[maybe_unused]] bool const success = event.session->ScheduleSend(event.message);
    assert(success);
}

//------------------------------------------------------------------------------------------------

std::shared_ptr<Network::TCP::Session> Network::TCP::Endpoint::CreateSession()
{
    auto spSession = std::make_shared<Network::TCP::Session>(m_context);

    spSession->OnMessageDispatched(
        [this] (auto const& spSession) { OnMessageDispatched(spSession); });
    spSession->OnMessageReceived(
        [this] (auto const& spSession, auto const& source, auto message) -> bool
        { return OnMessageReceived(spSession, source, message); });
    spSession->OnSessionError(
        [this] (auto const& spSession) { OnSessionStopped(spSession); });

    return spSession;
}

//------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::Bind(
    NetworkUtils::Address const& address, NetworkUtils::Port port)
{
    std::ostringstream entry;
    entry << address << NetworkUtils::ComponentSeperator << port;

    auto const OnBindError = [&acceptor = m_acceptor] () -> bool
    {
        acceptor.close();
        return false;
    };

    {
        std::ostringstream notification;
        notification << "[TCP] Opening TCP Server on: " << entry.str();
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
    }

    if (m_acceptor.is_open()) { m_acceptor.cancel(); }

    boost::system::error_code error;
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string(address), port);

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
        m_entry = entry.str();
    }

    return true;
}

//------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::Listen()
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
            if (IsInducedError(error)) { co_return TCP::CompletionOrigin::Self; }

            std::ostringstream notification;
            notification << "[TCP] Error occured accepting connection!";
            NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
            co_return TCP::CompletionOrigin::Error;
        }

        OnSessionStarted(spSession);
    }

    co_return TCP::CompletionOrigin::Self;
}

//------------------------------------------------------------------------------------------------

Network::TCP::ConnectStatusCode Network::TCP::Endpoint::Connect(
    NetworkUtils::Address const& address,
    NetworkUtils::Port port,
    BryptIdentifier::SharedContainer const& spIdentifier)
{
    if (!m_pPeerMediator) { return ConnectStatusCode::GenericError; }

    std::string uri;
    uri.append(Scheme.data());
    uri.append(address);
    uri.append(NetworkUtils::ComponentSeperator);
    uri.append(std::to_string(port));

    auto const status = local::IsAllowableUri(uri, m_pEndpointMediator, m_tracker);
    if (status != ConnectStatusCode::Success) { return status; }

    {
        std::ostringstream notification;
        notification << "[TCP] Connecting to " << uri;
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
    }

    boost::asio::co_spawn(
        m_context, Connect(uri, address, port, spIdentifier),
        [uri] (std::exception_ptr exception, TCP::CompletionOrigin origin)
        {
            bool const error = (exception || origin == TCP::CompletionOrigin::Error);
            if (error) {
                std::ostringstream notification;
                notification << "[TCP] An error caused the conncection attempt to " << uri;
                notification << " to fail!";
                NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
            }
        });

    return ConnectStatusCode::Success;
}

//------------------------------------------------------------------------------------------------

Network::TCP::SocketProcessor Network::TCP::Endpoint::Connect(
    std::string uri,
    std::string address,
    NetworkUtils::Port port,
    BryptIdentifier::SharedContainer spIdentifier)
{
    boost::system::error_code error;

    auto const endpoints = co_await m_resolver.async_resolve(
        { address, std::to_string(port) },
        boost::asio::redirect_error(boost::asio::use_awaitable, error));

    if (error) {
        std::ostringstream notification;
        notification << "[TCP] An endpoint could not be resolved for " << address;
        notification << NetworkUtils::ComponentSeperator << port << "!";
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
        co_return TCP::CompletionOrigin::Error;
    }

    // Get the connection request message from the peer mediator. If we have not been given 
    // an expected identifier declare a new peer using the peer's URI. Otherwise, declare 
    // the resolving peer using the expected identifier. If the peer is known through the 
    // identifier, the mediator will provide us a heartbeat request to verify the endpoint.
    std::optional<std::string> optConnectionRequest = 
        (!spIdentifier) ? m_pPeerMediator->DeclareResolvingPeer(uri) :
                          m_pPeerMediator->DeclareResolvingPeer(spIdentifier);

    if (!optConnectionRequest) {
        std::ostringstream notification;
        notification << "[TCP] The initial connection request could not be determined for";
        notification << address << NetworkUtils::ComponentSeperator << port << "!";;
        NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
        co_return TCP::CompletionOrigin::Error;
    }

    auto const spSession = CreateSession();
    co_await boost::asio::async_connect(
        spSession->GetSocket(),
        endpoints,
        boost::asio::redirect_error(boost::asio::use_awaitable, error));
    
    // If there was an error establishing the connection, there is nothing to do. 
    if (error) { co_return TCP::CompletionOrigin::Error; }

    OnSessionStarted(spSession);
    
    if (!spSession->ScheduleSend(*optConnectionRequest)) {
        co_return TCP::CompletionOrigin::Error;
    }

    // Send the initial request to the peer. 
    co_return TCP::CompletionOrigin::Self;
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnSessionStarted(std::shared_ptr<Session> const& spSession)
{
    // Initialize the session.
    spSession->Initialize();

    // Start tracking the descriptor for communication.
    m_tracker.TrackConnection(spSession, spSession->GetURI());

    // Start the session handlers. 
    spSession->Start();

    // Log that a new session has started with a new peer. 
    std::ostringstream notification;
    notification << "[TCP] Session started with " << spSession->GetURI();
    NodeUtils::printo(notification.str(), NodeUtils::PrintType::Endpoint);
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnSessionStopped(std::shared_ptr<Session> const& spSession)
{
    auto const updater = [this] (ExtendedConnectionDetails& details)
    { 
        details.SetConnectionState(ConnectionState::Disconnected);
        if (auto const spBryptPeer = details.GetBryptPeer(); spBryptPeer) {
            spBryptPeer->WithdrawEndpoint(m_identifier, m_protocol);
        }
    };

    m_tracker.UpdateOneConnection(spSession, updater);

    if (m_active) { m_tracker.UntrackConnection(spSession); }
}

//------------------------------------------------------------------------------------------------

bool Network::TCP::Endpoint::OnMessageReceived(
    std::shared_ptr<Session> const& spSession,
    BryptIdentifier::Container const& source,
    std::span<std::uint8_t const> message)
{
    std::shared_ptr<BryptPeer> spBryptPeer = {};
    auto const promotedHandler = [&spBryptPeer] (ExtendedConnectionDetails& details)
    {
        spBryptPeer = details.GetBryptPeer();
        details.IncrementMessageSequence();
    };

    auto const unpromotedHandler = [this, &source, &spBryptPeer] (std::string_view uri) 
        -> ExtendedConnectionDetails
    {
        spBryptPeer = IEndpoint::LinkPeer(source, uri);
        
        ExtendedConnectionDetails details(spBryptPeer);
        details.SetConnectionState(ConnectionState::Connected);
        details.IncrementMessageSequence();
        
        spBryptPeer->RegisterEndpoint(m_identifier, m_protocol, m_scheduler, uri);

        return details;
    };

    // Update the information about the node as it pertains to received data. The node may not be
    // found if this is its first connection.
    m_tracker.UpdateOneConnection(spSession, promotedHandler, unpromotedHandler);

    return spBryptPeer->ScheduleReceive(m_identifier, message);
}

//------------------------------------------------------------------------------------------------

void Network::TCP::Endpoint::OnMessageDispatched(std::shared_ptr<Session> const& spSession)
{
    auto const updater = [] (ExtendedConnectionDetails& details)
        { details.IncrementMessageSequence(); };

    m_tracker.UpdateOneConnection(spSession, updater);
}

//------------------------------------------------------------------------------------------------

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
    if (bool const tracked = tracker.IsURITracked(uri); tracked) {
        std::ostringstream oss;
        oss << "[TCP] Duplicate connection to " << uri;
        NodeUtils::printo(oss.str(), NodeUtils::PrintType::Endpoint);
        return ConnectStatusCode::DuplicateError;
    }

    return ConnectStatusCode::Success;
}

//------------------------------------------------------------------------------------------------
