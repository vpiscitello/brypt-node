//------------------------------------------------------------------------------------------------
// File: StreamBridgeEndpoint.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "StreamBridgeEndpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "EndpointConstants.hpp"
#include "ZmqContextPool.hpp"
#include "../../Utilities/Message.hpp"
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

using namespace StreamBridge;

//------------------------------------------------------------------------------------------------

Endpoints::CStreamBridgeEndpoint::CStreamBridgeEndpoint(
    IMessageSink* const messageSink,
    Configuration::TEndpointOptions const& options)
    : CEndpoint(messageSink, options)
    , m_address()
    , m_port()
    , m_peers()
    , m_outgoingMutex()
    , m_outgoing()
{
    if (m_operation != NodeUtils::EndpointOperation::Server) {
        throw std::runtime_error("StreamBridge endpoint may only operate in server mode.");
    }

    // Get the two networking components from the supplied options
    auto const [boundAddress, boundPort] = options.GetBindingComponents();
    m_address = boundAddress; // The first binding component is the address for the interface
    m_port = boundPort; // The second binding component is the port
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
std::string Endpoints::CStreamBridgeEndpoint::GetProtocolType() const
{
    return ProtocolType.data();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType Endpoints::CStreamBridgeEndpoint::GetInternalType() const
{
    return InternalType;
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
void Endpoints::CStreamBridgeEndpoint::HandleProcessedMessage(
    NodeUtils::NodeIdType id, CMessage const& message)
{
    Send(id, message); // Forward the message pack to be sent on the socket
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::Send(NodeUtils::NodeIdType id, CMessage const& message)
{
    Send(id, message.GetPack()); // Forward the message pack to be sent on the socket
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CStreamBridgeEndpoint::Send(NodeUtils::NodeIdType id, std::string_view message)
{
    // If the message provided is empty, do not send anything
    if (message.empty()) {
        return;
    }

    auto const optIdentity = m_peers.Translate(id);
    if (!optIdentity) {
        return;
    }

    {
        std::scoped_lock lock(m_outgoingMutex);
        m_outgoing.emplace_back(*optIdentity, message, 0);
    }
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

    m_terminate = true; // Stop the worker thread from processing the connections
    m_cv.notify_all(); // Notify the worker that exit conditions have been set
    
    if (m_worker.joinable()) {
        m_worker.join();
    }

    m_peers.ReadEachPeer(
        [&]([[maybe_unused]] auto const& identity, auto const& optDetails) -> CallbackIteration
        {
            // If the connection has an attached node, unpublish the callback from Brypt Core
            if (optDetails) {
                m_messageSink->UnpublishCallback(optDetails->GetNodeId());
            }
            return CallbackIteration::Continue;
        }
    );
    
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
        case NodeUtils::EndpointOperation::Server: {
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
    NodeUtils::printo("[StreamBridge] Setting up STREAM socket on port " + m_port, NodeUtils::PrintType::Endpoint);

    // Instanties a ZMQ_STREAM socket local to this thread. Per the ZMQ documentation, sockets are not 
    // thread safe. These sockets should only manipulated within the worker thread that creates it.
    std::shared_ptr<zmq::context_t> spContext = ZmqContextPool::Instance().GetContext();
    thread_local zmq::socket_t socket(*spContext, zmq::socket_type::stream);
    Listen(socket, m_address, m_port);  // Start listening on the socket

    // Notify the calling thread that the endpoint worker is ready
    m_active = true;
    m_cv.notify_all();

    // Start the endpoint's event loop
    while(m_active) {
        ProcessIncomingMessages(socket); // Process any messages on the socket to be handled this cycle 
        ProcessOutgoingMessages(socket); // Send messages from the outgoing message queue
        
        // Gracefully handle thread termination by waiting a period of time for a terminate 
        // signal before continuing normal operation. 
        {
            std::unique_lock lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + Endpoint::CycleTimeout;
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
void Endpoints::CStreamBridgeEndpoint::Listen(
    zmq::socket_t& socket,
    NodeUtils::NetworkAddress const& address,
    NodeUtils::PortNumber const& port)
{
    // Set a socket option that indicates once the socket is destroyed it should not linger in the
    // ZMQ background threads. 
    socket.setsockopt(ZMQ_LINGER, 0);
    // Bind the ZMQ_STREAM socket to the designated interface and port
    socket.bind("tcp://" + address + ":" + port);
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
    if (!optIdenitityBytes) {
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
    NodeUtils::printo("[StreamBridge] Received request: " + std::string(message), NodeUtils::PrintType::Endpoint);
    try {
        CMessage const request(message);
        auto const id = request.GetSourceId();

        // Update the information about the node as it pertains to received data. The node may not be found 
        // if this is its first connection.
        bool const bPeerDetailsFound = m_peers.UpdateOnePeer(identity,
            [] (auto& details) {
                details.IncrementMessageSequence();
            }
        );

        // Ifd the node was not found then in the update then, we should start tracking the node.
        if (!bPeerDetailsFound) {
            // TODO: Peer should be registered with an authentication and key manager.
            CPeerDetails<> details(
                request.GetSourceId(),
                ConnectionState::Connected,
                MessagingPhase::Response);
            
            details.IncrementMessageSequence();
            m_peers.PromoteConnection(identity, details);

            // Register this peer's message handler with the message sink
            m_messageSink->RegisterCallback(id, this, &CEndpoint::HandleProcessedMessage);
        }

        // TODO: Only authenticated requests should be forwarded into BryptNode
        m_messageSink->ForwardMessage(id, request);
    } catch (...) {
        NodeUtils::printo("[StreamBridge] Received message failed to unpack.", NodeUtils::PrintType::Endpoint);
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
        std::scoped_lock lock(m_outgoingMutex);
        for (std::uint32_t idx = 0; idx < Endpoint::OutgoingMessageLimit; ++idx) {
            // If there are no messages left in the outgoing message queue, break from
            // copying the items into the temporary queue.
            if (m_outgoing.size() == 0) {
                break;
            }
            outgoing.emplace_back(m_outgoing.front());
            m_outgoing.pop_front();
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
            if (retries == Endpoint::MessageRetryLimit) {
                // TODO: Logic is needed to properly handling this condition. Should the peer be flagged?
                // Should the response required be flipped?
                continue;
            }
            std::uint8_t const attempts = retries + 1;
            std::scoped_lock lock(m_outgoingMutex);
            m_outgoing.emplace_back(identity, message, attempts);
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
    ZeroMQIdentity identity,
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

    NodeUtils::printo("[StreamBridge] Sent: (" + std::to_string(*optResult) + ") " 
           + message.data(), NodeUtils::PrintType::Endpoint);

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
            auto const id = details.GetNodeId();

            switch (details.GetConnectionState()) {
                case ConnectionState::Connected: {
                    details.SetConnectionState(ConnectionState::Disconnected);   
                    m_messageSink->UnpublishCallback(id);
                } break;
                case ConnectionState::Disconnected: {
                    // TODO: Previously disconnected nodes should be re-authenticated prior to re-adding
                    // their callbacks with BryptNode
                    details.SetConnectionState(ConnectionState::Connected);
                    m_messageSink->RegisterCallback(id, this, &CEndpoint::HandleProcessedMessage);
                } break;
                // Other ConnectionStates are not currently handled for this endpoint
                default: assert(true); break;
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