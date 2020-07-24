//------------------------------------------------------------------------------------------------
// File: StreamBridgeEndpoint.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "StreamBridgeEndpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "Peer.hpp"
#include "EndpointDefinitions.hpp"
#include "ZmqContextPool.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
#include "../../Utilities/VariantVisitor.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <optional>
#include <utility>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

void ShutdownSocket(zmq::socket_t& socket);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

Endpoints::CStreamBridgeEndpoint::CStreamBridgeEndpoint(
    NodeUtils::NodeIdType id,
    std::string_view interface,
    Endpoints::OperationType operation,
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator,
    IMessageSink* const pMessageSink)
    : CEndpoint(
        id, interface, operation, pEndpointMediator, pPeerMediator, pMessageSink, TechnologyType::StreamBridge)
    , m_address()
    , m_port()
    , m_peers()
    , m_eventsMutex()
    , m_events()
{
    if (m_operation != OperationType::Server) {
        throw std::runtime_error("StreamBridge endpoint may only operate in server mode.");
    }
    
    if (m_pMessageSink) {
        auto callback = [this] (CMessage const& message) -> bool { return ScheduleSend(message); };  
        m_pMessageSink->RegisterCallback(m_identifier, callback);
    }
}

//------------------------------------------------------------------------------------------------

Endpoints::CStreamBridgeEndpoint::~CStreamBridgeEndpoint()
{
    bool const success = Shutdown();
    if (!success) {
        m_worker.detach();
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Endpoints::TechnologyType Endpoints::CStreamBridgeEndpoint::GetInternalType() const
{
    return InternalType;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::string Endpoints::CStreamBridgeEndpoint::GetProtocolType() const
{
    return ProtocolType.data();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns:
//------------------------------------------------------------------------------------------------
std::string Endpoints::CStreamBridgeEndpoint::GetEntry() const
{
    std::string entry = "";
    if (m_address.empty()) {
        return entry;
    }

    entry.append(m_address);
    entry.append(NetworkUtils::ComponentSeperator.data());
    entry.append(std::to_string(m_port));

    return entry;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns:
//------------------------------------------------------------------------------------------------
std::string Endpoints::CStreamBridgeEndpoint::GetURI() const
{
    return Scheme.data() + GetEntry();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::ScheduleBind(std::string_view binding)
{
    if (m_operation != OperationType::Server) {
        throw std::runtime_error("Bind was called a non-listening Endpoint!");
    }

    auto [address, sPort] = NetworkUtils::SplitAddressString(binding);
    if (address.empty() || sPort.empty()) {
        return;
    }

    NetworkUtils::PortNumber port = std::stoul(sPort);

    if (auto const found = address.find(NetworkUtils::Wildcard); found != std::string::npos) {
        address = NetworkUtils::GetInterfaceAddress(m_interface);
    }

    // Schedule the Bind network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(
            StreamBridge::TNetworkInstructionEvent(NetworkInstruction::Bind, address, port));
    }

    {
        std::scoped_lock lock(m_mutex);
        m_address = address; // Capture the address the endpoint bound too
        m_port = port; // Capture the port the endpoint bound too
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::ScheduleConnect([[maybe_unused]] std::string_view entry)
{
    throw std::runtime_error("Connect was called a non-client Endpoint!");
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::Startup()
{
    if (m_active) {
        return; 
    }
    Spawn(); // Spawn a new worker thread that will handle message receivin
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CStreamBridgeEndpoint::ScheduleSend(CMessage const& message)
{
    // Forward the message pack to be sent on the socket
    return ScheduleSend(message.GetDestination(), message.GetPack());
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CStreamBridgeEndpoint::ScheduleSend(NodeUtils::NodeIdType id, std::string_view message)
{
    // If the message provided is empty, do not send anything
    if (message.empty()) {
        return false;
    }

    auto const optIdentity = m_peers.Translate(id);
    if (!optIdentity) {
        return false;
    }

    // Schedule the outgoing message event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(
            StreamBridge::TOutgoingMessageEvent(*optIdentity, message, 0));
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CStreamBridgeEndpoint::Shutdown()
{
    if (!m_active) {
        return true;
    }

    NodeUtils::printo("[StreamBridge] Shutting down endpoint", NodeUtils::PrintType::Endpoint);
    if (m_pMessageSink) {
        m_pMessageSink->UnpublishCallback(m_identifier);
    }

    m_terminate = true; // Stop the worker thread from processing the connections
    m_cv.notify_all(); // Notify the worker that exit conditions have been set
    
    if (m_worker.joinable()) {
        m_worker.join();
    }
    
    return !m_worker.joinable();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::Spawn()
{
    bool bWorkerStarted = false;

    // Attempt to start an endpoint worker thread based on the designated endpoint operation.
    // Currently, StreamBridge endpoints may only operate as a server.
    switch (m_operation) {
        case OperationType::Server: {
            bWorkerStarted = SetupServerWorker();
        } break;
        default: return;
    }

    // If the endpoint worker thread failed to start return without waiting for it
    if (!bWorkerStarted) {
        return;
    }

    // Wait for the spawned thread to signal it is ready to be used
    std::unique_lock lock(m_mutex);
    this->m_cv.wait(lock, [this]{ return m_active.load(); });
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CStreamBridgeEndpoint::SetupServerWorker()
{
    std::scoped_lock lock(m_mutex);
    m_worker = std::thread(&CStreamBridgeEndpoint::ServerWorker, this); // Create the worker thread
    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::ServerWorker()
{
    // Instanties a ZMQ_STREAM socket local to this thread. Per the ZMQ documentation, sockets are not 
    // thread safe. These sockets should only manipulated within the worker thread that creates it.
    std::shared_ptr<zmq::context_t> spContext = ZmqContextPool::Instance().GetContext();
    thread_local zmq::socket_t socket(*spContext, zmq::socket_type::stream);
    // Set a socket option that indicates once the socket is destroyed it should not linger in the
    // ZMQ background threads. 
    socket.setsockopt(ZMQ_LINGER, 0);

    // Notify the calling thread that the endpoint worker is ready
    m_active = true;
    m_cv.notify_all();

    // Start the endpoint's event loop
    while(m_active) {
        ProcessNetworkInstructions(socket); // Process any endpoint instructions at the front if the event queue
        ProcessIncomingMessages(socket); // Process any messages on the socket to be handled this cycle 
        ProcessOutgoingMessages(socket); // Send messages from the outgoing message queue
        
        // Gracefully handle thread termination by waiting a period of time for a terminate 
        // signal before continuing normal operation. 
        {
            std::unique_lock lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + Endpoints::CycleTimeout;
            m_cv.wait_until(lock, stop, [&]{ return m_terminate.load(); });
            if (m_terminate) {
                m_active = false; // Terminate if the endpoint is shutting down
            }
        }
    }

    local::ShutdownSocket(socket);  // Shutdown the ZMQ Socket
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CStreamBridgeEndpoint::Listen(
    zmq::socket_t& socket,
    NetworkUtils::NetworkAddress const& address,
    NetworkUtils::PortNumber port)
{
    std::string uri;
    uri.append(Scheme.data());
    uri.append(address);
    uri.append(NetworkUtils::ComponentSeperator);
    uri.append(std::to_string(port));

    printo("[StreamBridge] Setting up ZMQ_STREAM socket on " + uri, NodeUtils::PrintType::Endpoint);

    std::size_t size = 128; // Initalize a size of the buffer
    std::vector<char> buffer(size, 0); // Initialize the receiving buffer

    try {
        socket.getsockopt(ZMQ_LAST_ENDPOINT, buffer.data(), &size); // Get the last endpoint the socket was bound too
        // If the socket was previously bound, unbind the socket from the current interface
        if (size > 1) {
            m_address.clear(); // Clear the bound address of the endpoint
            m_port = 0; // Clear the bound port of the endpoint

            socket.unbind(buffer.data()); // Unbind the socket
        }
        
        // Bind the ZMQ_ROUTER socket to the designated interface and port
        socket.bind(uri);
    } catch (...) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::ProcessNetworkInstructions(zmq::socket_t& socket)
{
    // Splice elements up to send cycle limit into a temporary queue to be sent.
    NetworkInstructionDeque instructions;
    {
        std::scoped_lock lock(m_eventsMutex);

        // Splice all network instruction events until the first non-instruction event
        // TODO: Should looping be flagged here? The endpoint could in theory be DOS'd by maniuplating
        // connect calls, how/why would this happen?
        while (m_events.size() != 0 && 
               m_events.front().type() == typeid(StreamBridge::TNetworkInstructionEvent)) {

            instructions.emplace_back(
                std::any_cast<StreamBridge::TNetworkInstructionEvent>(m_events.front()));

            m_events.pop_front();
        }
    }

    for (auto const& [type, address, port] : instructions) {
        switch (type) {
            case NetworkInstruction::Bind: {
                bool const success = Listen(socket, address, port);  // Start listening on the socket
                if (!success) {
                    std::string const notification = "[StreamBridge] Binding to " + address + ":" + std::to_string(port) + " failed.";
                    NodeUtils::printo(notification, NodeUtils::PrintType::Endpoint);   
                }
            } break;
            default: assert(false); break; // What is this?
        }
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::ProcessIncomingMessages(zmq::socket_t& socket)
{
    // Attempt to receive a request on our socket while not blocking.
    auto const optResult = Receive(socket);
    if (!optResult) {
        return;
    }

    // If a request has been received, forward the message through the message sink and
    // wait until the message has been processed before accepting another message
    auto const& [identity, result] = *optResult;
    std::visit(TVariantVisitor
        {
            [this, &identity](ConnectionStateChange change)
            {
                HandleConnectionStateChange(identity, change);
            },
            [this, &identity](std::string_view message) {
                HandleReceivedData(identity, message);
            },
        },
        result);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Endpoints::CStreamBridgeEndpoint::OptionalReceiveResult Endpoints::CStreamBridgeEndpoint::Receive(
    zmq::socket_t& socket)
{
    zmq::message_t message;
    
    // The first message from the socket will be the internal ZMQ identity used to identifiy a peer
    // If we don't receive an idenity we should return nullopt.
    auto const optIdenitityBytes = socket.recv(message, zmq::recv_flags::dontwait);
    if (!optIdenitityBytes || *optIdenitityBytes != 5) {
        return {};
    }
    assert(*optIdenitityBytes == 5); // We should expect the idenity received to be exactly five bytes
    Endpoints::CStreamBridgeEndpoint::ZeroMQIdentity const identity(static_cast<char*>(message.data()), *optIdenitityBytes);

    // The second message from the socket should be either an zero length message (indicating a connect or disconnect)
    // or an actual message. We should block this receive to ensure we get the data associated with the identiy.
    auto const optDataBytes = socket.recv(message, zmq::recv_flags::none);
    if (!optDataBytes) {
        return {};
    }

    // If the bytes received contains zero, we have received peer state change and should signal the processing
    // code to update basesharedd on known information.
    if (*optDataBytes == 0) {
        return {{ identity, ConnectionStateChange::Update }};
    }

    // Update the network message pattern tracker for the requesting peer. We should expect 
    // the current request is allowed, however, a check must be made to ensure the peer is 
    // adhering to the request/reply structure. 
    bool bMessageAllowed = true;
    [[maybe_unused]] bool const bPeerDetailsFound = m_peers.UpdateOnePeer(identity,
        [&bMessageAllowed](auto& details) {
            // TODO: A generic message filtering service should be used to ensure all connection interfaces
            // use the same message allowance scheme. 
            // TODO: If the peer is breaking protocol should it be flagged?
            ConnectionState state = details.GetConnectionState();
            MessagingPhase phase = details.GetMessagingPhase();
            if (state != ConnectionState::Flagged && phase != MessagingPhase::Request) {
                bMessageAllowed = false;
                return;
            }
            details.SetMessagingPhase(MessagingPhase::Response);
        }
    );

    // If a new request is not allowed from this peer 
    if (!bMessageAllowed) {
        return {};
    }

    // At this point we should be able to resonably expect some data was received to be processed.
    std::string const request = std::string(static_cast<char*>(message.data()), *optDataBytes);
    return {{ identity, request }};
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::HandleReceivedData(
    ZeroMQIdentity const& identity,
    std::string_view message)
{
    NodeUtils::printo("[StreamBridge] Received message: " + std::string(message), NodeUtils::PrintType::Endpoint);

    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext({ m_identifier, m_technology })
        .FromPack(message)
        .ValidatedBuild();

    if (!optRequest) {
        return;
    }

    // Update the information about the node as it pertains to received data. The node may not be found 
    // if this is its first connection.
    m_peers.UpdateOnePeer(identity,
        [] (auto& details) {
            details.IncrementMessageSequence();
        },
        [this, &optRequest] (std::string_view uri, auto& optDetails) -> PeerResolutionCommand {
            optDetails = ExtendedPeerDetails(optRequest->GetSource());

            optDetails->SetConnectionState(ConnectionState::Connected);
            optDetails->SetMessagingPhase(MessagingPhase::Response);
            optDetails->IncrementMessageSequence();

            CEndpoint::PublishPeerConnection({optRequest->GetSource(), InternalType, uri});

            return PeerResolutionCommand::Promote;
        }
    );

    if (m_pMessageSink) {
        m_pMessageSink->ForwardMessage(*optRequest);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::ProcessOutgoingMessages(zmq::socket_t& socket)
{
    // Splice elements up to send cycle limit into a temporary queue to be sent.
    OutgoingMessageDeque outgoing;
    {
        std::scoped_lock lock(m_eventsMutex);
        for (std::uint32_t idx = 0; idx < Endpoints::OutgoingMessageLimit; ++idx) {
            // If there are no messages left in the outgoing message queue, break from
            // copying the items into the temporary queue.
            if (m_events.size() == 0) {
                break;
            }

            // If we have reached an event that is not an outgoing message stop splicing
            auto const& eventType = m_events.front().type();
            if (eventType != typeid(StreamBridge::TOutgoingMessageEvent)) {
                break;
            }

            outgoing.emplace_back(
                std::any_cast<StreamBridge::TOutgoingMessageEvent>(m_events.front()));

            m_events.pop_front();
        }
    }

    for (auto const& [identity, message, retries] : outgoing) {

        // Determine if the message being sent is allowed given the current state of communications
        // with the peer. Brypt networking structure dictates that each request is coupled with a response.
        MessagingPhase phase = MessagingPhase::Response;
        m_peers.ReadOnePeer(identity,
            [&phase] (auto const& details) {
                phase = details.GetMessagingPhase();
            }
        );

        // If the current message is not allowed in the current network context, skip to the next message.
        // TODO: Should something happen if Brypt Core is trying to send a message to this peer outside of protocol
        if (phase != MessagingPhase::Response) {
            continue;
        }

        std::uint32_t const bytesSent = Send(socket, identity, message);  // Attempt to send the message

        // If the message was sent, update relevant peer tracking details
        // Otherwise, schedule another send attempt if possible
        if (bytesSent > 0) {
            m_peers.UpdateOnePeer(identity,
                [] (auto& details) {
                    details.IncrementMessageSequence();
                    details.SetMessagingPhase(MessagingPhase::Request);
                }
            );
        } else {
            // If we have already attempted to send the message three times, drop the message.
            if (retries == Endpoints::MessageRetryLimit) {
                // TODO: Logic is needed to properly handling this condition. Should the peer be flagged?
                // Should the response required be flipped?
                continue;
            }
            std::uint8_t const attempts = retries + 1;
            {
                std::scoped_lock lock(m_eventsMutex);
                m_events.emplace_back(
                    StreamBridge::TOutgoingMessageEvent(identity, message, attempts));
            }
        }

        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::uint32_t Endpoints::CStreamBridgeEndpoint::Send(
    zmq::socket_t& socket,
    ZeroMQIdentity const& identity,
    std::string_view message)
{
    // Create a zmq message from the provided data and send it
    zmq::message_t identityMessage(identity.data(), identity.size());
    zmq::message_t requestMessage(message.data(), message.size());
    
    // First we shall send the identity to signal to ZMQ which peer we are sending to
    socket.send(identityMessage, zmq::send_flags::sndmore);
    auto const optResult = socket.send(requestMessage, zmq::send_flags::dontwait);
    if (!optResult) {
        return 0;
    }

    NodeUtils::printo("[StreamBridge] Sent: " + std::string(message.data()), NodeUtils::PrintType::Endpoint);

    return *optResult;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::HandleConnectionStateChange(
    ZeroMQIdentity const& identity,
    [[maybe_unused]] ConnectionStateChange change)
{
    bool const bPeerDetailsFound = m_peers.UpdateOnePeer(identity,
        [&] (auto& details)
        {
            switch (details.GetConnectionState()) {
                case ConnectionState::Connected: {
                    details.SetConnectionState(ConnectionState::Disconnected);
                    CEndpoint::UnpublishPeerConnection({details.GetNodeId(), InternalType, details.GetURI()});
                } break;
                case ConnectionState::Disconnected: {
                    // TODO: Previously disconnected nodes should be re-authenticated prior to re-adding
                    // their callbacks with BryptNode
                    details.SetConnectionState(ConnectionState::Connected);
                    CEndpoint::PublishPeerConnection({details.GetNodeId(), InternalType, details.GetURI()});
                } break;
                case ConnectionState::Resolving: break;
                // Other ConnectionStates are not currently handled for this endpoint
                default: assert(false); break;
            }
        }
    );

    if (!bPeerDetailsFound) {
        m_peers.TrackConnection(identity);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void local::ShutdownSocket(zmq::socket_t& socket)
{
    if (socket.connected()) {
        socket.close();
    }
}

//------------------------------------------------------------------------------------------------