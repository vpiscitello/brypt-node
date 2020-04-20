//------------------------------------------------------------------------------------------------
// File: TcpConnection.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "TcpEndpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "EndpointDefinitions.hpp"
#include "../Command/CommandDefinitions.hpp"
#include "../../Utilities/Message.hpp"
#include "../../Utilities/VariantVisitor.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <cerrno>
#include <chrono>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

constexpr std::uint8_t EnabledOption = 1;
constexpr std::uint8_t DisabledOption = 0;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

Endpoints::CTcpEndpoint::CTcpEndpoint(
    IMessageSink* const messageSink,
    Configuration::TEndpointOptions const& options)
    : CEndpoint(messageSink, options)
    , m_address()
    , m_port(0)
    , m_peers()
    , m_eventsMutex()
    , m_events()
{
    switch (m_operation) {
        case NodeUtils::EndpointOperation::Server: {
            auto const binding = options.GetBinding();
            ScheduleBind(binding);
        } break;
        case NodeUtils::EndpointOperation::Client: {
            auto const entry = options.GetEntry();
            if (!entry.empty()) {
                ScheduleConnect(entry);
            }
        } break;
        default: assert(false); break; // What is this?
    }
}

//------------------------------------------------------------------------------------------------

Endpoints::CTcpEndpoint::~CTcpEndpoint()
{
    bool const success = Shutdown();
    if (!success) {
        m_worker.detach();
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType Endpoints::CTcpEndpoint::GetInternalType() const
{
    return InternalType;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns:
//------------------------------------------------------------------------------------------------
std::string Endpoints::CTcpEndpoint::GetProtocolType() const
{
    return ProtocolType.data();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns:
//------------------------------------------------------------------------------------------------
std::string Endpoints::CTcpEndpoint::GetEntry() const
{
    return m_address + NetworkUtils::Wildcard.data() + std::to_string(m_port);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ScheduleBind(std::string_view binding)
{
    if (m_operation != NodeUtils::EndpointOperation::Server) {
        throw std::runtime_error("Bind was called a non-listening Endpoint!");
    }

    auto [address, sPort] = NetworkUtils::SplitAddressString(binding);
    NetworkUtils::PortNumber port = std::stoul(sPort);

    if (auto const found = address.find(NetworkUtils::Wildcard); found != std::string::npos) {
        address = NetworkUtils::GetInterfaceAddress(m_interface);
    }

    // Schedule the Bind network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(
            Tcp::TNetworkInstructionEvent(NetworkInstruction::Bind, address, port));
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ScheduleConnect(std::string_view entry)
{
    if (m_operation != NodeUtils::EndpointOperation::Client) {
        throw std::runtime_error("Connect was called a non-client Endpoint!");
    }
    
    auto const [address, sPort] = NetworkUtils::SplitAddressString(entry);
    NetworkUtils::PortNumber port = std::stoul(sPort);

    // Schedule the Connect network instruction event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(
            Tcp::TNetworkInstructionEvent(NetworkInstruction::Connect, address, port));
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::Startup()
{
    if (m_active) {
        return; 
    }
    Spawn();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::HandleProcessedMessage(NodeUtils::NodeIdType id, CMessage const& message)
{
    ScheduleSend(id, message.GetPack()); // Forward the received message to be sent on the socket
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ScheduleSend(NodeUtils::NodeIdType id, CMessage const& message)
{
    ScheduleSend(id, message.GetPack()); // Forward the received message to be sent on the socket
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ScheduleSend(NodeUtils::NodeIdType id, std::string_view message)
{
    // If the message provided is empty, do not send anything
    if (message.empty()) {
        return;
    }

    // Get the socket descriptor of the intended destination. If it is not found, drop the message.
    auto const optDescriptor = m_peers.Translate(id);
    if (!optDescriptor) {
        return;
    }

    // Schedule the outgoing message event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(
            Tcp::TOutgoingMessageEvent(*optDescriptor, message, 0));
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CTcpEndpoint::Shutdown()
{
    if (!m_active) {
        return true;
    }

    NodeUtils::printo("[TCP] Shutting down endpoint", NodeUtils::PrintType::Endpoint);
     
    m_terminate = true; // Stop the worker thread from processing the connections
    m_cv.notify_all(); // Notify the worker that exit conditions have been set
    
    if (m_worker.joinable()) {
        m_worker.join();
    }

    m_peers.ReadEachPeer(
        [&]([[maybe_unused]] auto const& descriptor, auto const& optDetails) -> CallbackIteration
        {
            // If the connection has an attached node, unpublish the callback from Brypt Core
            if (optDetails) {
                m_messageSink->UnpublishCallback(optDetails->GetNodeId());
            }
            ::close(descriptor); // Close the connection descriptor
            return CallbackIteration::Continue;
        }
    );
    
    return !m_worker.joinable();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::Spawn()
{
    bool bWorkerStarted = false;

    // Attempt to start an endpoint worker thread based on the designated endpoint operation.
    switch (m_operation) {
        case NodeUtils::EndpointOperation::Server: {
            bWorkerStarted = SetupServerWorker(); 
        } break;
        case NodeUtils::EndpointOperation::Client: {
            bWorkerStarted = SetupClientWorker();
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
bool Endpoints::CTcpEndpoint::SetupServerWorker()
{
    m_worker = std::thread(&CTcpEndpoint::ServerWorker, this); // Create the worker thread 
    std::scoped_lock lock(m_mutex);
    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ServerWorker()
{
    SocketDescriptor listenDescriptor = Tcp::InvalidDescriptor;
    
    // Notify the calling thread that the connection worker is ready
    m_active = true;
    m_cv.notify_all();

    // Start the endpoint's event loop 
    while(m_active) {
        ProcessNetworkInstructions(&listenDescriptor); // Process any endpoint instructions at the front if the event queue
        AcceptConnection(listenDescriptor);
        ProcessIncomingMessages(); // Process any messages on the socket to be handled this cycle 
        ProcessOutgoingMessages(); // Send messages from the outgoing message queue
        
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

    ::close(listenDescriptor);  // Close the server thread's local listen socket
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CTcpEndpoint::Listen(
    SocketDescriptor* listener,
    NetworkUtils::NetworkAddress const& address,
    NetworkUtils::PortNumber port,
    IPv4SocketAddress& socketAddress)
{
    NodeUtils::printo("[TCP] Setting up TCP socket on port " + std::to_string(port), NodeUtils::PrintType::Endpoint);

    // If a listener socket was not provided to perform the bind instruction, don't do anything.
    if (listener == nullptr) {
        return false;
    }

    // If the listener is not an invalid descriptor, first close the current socket.
    if (*listener != Tcp::InvalidDescriptor) {
        m_address.clear(); // Clear the bound address of the endpoint
        m_port = 0; // Clear the bound port of the endpoint

        ::close(*listener); // Close the listener
        // The provided listener should be set to the InvalidDescriptor to indicate it is not yet bound 
        *listener = Tcp::InvalidDescriptor; 
    }

    // Creating socket file descriptor
    SocketDescriptor descriptor = ::socket(AF_INET, SOCK_STREAM, 0); 
    if (descriptor < 0) {
        return false;
    }

    // Set the intended options for the listen socket. setsockopt and fcntl will return 0 on success and -1 on error.
    std::int32_t optionSetStatus = 0;
    optionSetStatus -= ::setsockopt(descriptor, SOL_SOCKET, SO_KEEPALIVE, &local::EnabledOption, 1);
    optionSetStatus -= ::setsockopt(descriptor, SOL_SOCKET, SO_REUSEADDR, &local::EnabledOption, 1);
    optionSetStatus -= ::setsockopt(descriptor, SOL_SOCKET, SO_REUSEPORT, &local::EnabledOption, 1);

    optionSetStatus -= ::fcntl(descriptor, F_SETFL, ::fcntl(descriptor, F_GETFL) | O_NONBLOCK);

    // If any of the socket options failed to be set return an invalid descriptor
    if (optionSetStatus < 0) {
        return false;
    }

    // Setup the socket address for binding on the listen descriptor
    socklen_t addressSize = SocketAddressSize;
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(port);
    if (::inet_pton(AF_INET, address.c_str(), &socketAddress.sin_addr) <= 0) {
        return false;
    }

    // Bind the socket address to the listen descriptor
    std::int32_t bindingResult = ::bind(
        descriptor,
        reinterpret_cast<sockaddr*>(&socketAddress),
        addressSize);
        
    if (bindingResult < 0) {
        return false;
    }

    std::int32_t listenResult = ::listen(descriptor, 30); // Start listening on the socket
    if (listenResult < 0) {
        return false;
    }
     
    *listener = descriptor; // Set the listener to the valid listening file descriptor 

    m_address = address; // Capture the address the endpoint bound too
    m_port = port; // Capture the port the endpoint bound too

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::AcceptConnection(SocketDescriptor listenDescriptor)
{
    // Create a static socket address, so we don't need to create it everytime we check the socket 
    static IPv4SocketAddress address = {}; // Zero initialize the socket address
    socklen_t size = SocketAddressSize;

    // Accept a connection that may be queued on the socket
    SocketDescriptor connectionDescriptor = ::accept(
        listenDescriptor,
        reinterpret_cast<sockaddr*>(&address),
        &size);

    // If an invalid descriptor has been returned by accept then return
    if (connectionDescriptor < 0) {
        return;
    }

    // Make the accepted socket non-blocking
    if (::fcntl(connectionDescriptor, F_SETFL, ::fcntl(connectionDescriptor, F_GETFL) | O_NONBLOCK) < 0) {
        return;
    }
    
    m_peers.TrackConnection(connectionDescriptor);

    std::memset(&address, 0, SocketAddressSize); // Reset the data contained in the socket address
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CTcpEndpoint::SetupClientWorker()
{
    std::scoped_lock lock(m_mutex);
    m_worker = std::thread(&CTcpEndpoint::ClientWorker, this); // Create the worker thread
     return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ClientWorker()
{
    m_active = true;
    m_cv.notify_all(); // Notify the calling thread that the connection worker is ready

    // Start the endpoint's event loop 
    while(m_active) {
        ProcessNetworkInstructions(); // Process any endpoint instructions at the front if the event queue
        ProcessIncomingMessages(); // Process any messages on the socket to be handled this cycle 
        ProcessOutgoingMessages(); // Send messages from the outgoing message queue
        
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
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CTcpEndpoint::Connect(
    NetworkUtils::NetworkAddress const& address,
    NetworkUtils::PortNumber port,
    IPv4SocketAddress& socketAddress)
{
    NodeUtils::printo("[TCP] Connecting TCP socket to " + address + ":" + std::to_string(port), NodeUtils::PrintType::Endpoint);

    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = ::htons(port);
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (::inet_pton(AF_INET, address.c_str(), &socketAddress.sin_addr) <= 0) {
        return false;
    }

    SocketDescriptor descriptor = std::numeric_limits<std::int32_t>::min();
    if (descriptor = ::socket(AF_INET, SOCK_STREAM, 0); descriptor < 0) {
        return false;
    }

    if (!EstablishConnection(descriptor, socketAddress)) {
        return false;
    }

    m_peers.TrackConnection(descriptor);  // Start tracking the descriptor for communication

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Endpoints::CTcpEndpoint::EstablishConnection(
    SocketDescriptor descriptor, 
    IPv4SocketAddress address)
{
    std::uint32_t attempts = 0;
    socklen_t size = SocketAddressSize;
    while (::connect(descriptor, reinterpret_cast<sockaddr *>(&address), size) < 0) {
        std::string error = "Unspecifed error";
        if (char const* const result = strerror(errno); result != nullptr) {
            error = std::string(result, strlen(result));
        } 

        NodeUtils::printo("[TCP] Connection to peer failed. Error: " + error, NodeUtils::PrintType::Endpoint);

        if (++attempts > Endpoint::ConnectRetryThreshold) {
            return false;
        }

        {
            std::unique_lock lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + Endpoint::ConnectRetryTimeout;
            m_cv.wait_until(lock, stop, [&]{ return m_terminate.load(); });
            if (m_terminate) {
                return false; // Terminate if the endpoint is shutting down
            }
        }
    }

    StartPeerAuthentication(descriptor);

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::StartPeerAuthentication(SocketDescriptor descriptor)
{
    // TODO: Implement better method of starting peer authentication
    CMessage message(
        m_id, 0x00000000,
        Command::Type::Connect, 0,
        "", 0);

    Send(descriptor, message.GetPack());
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ProcessNetworkInstructions(SocketDescriptor* listener)
{
    // Splice elements up to send cycle limit into a temporary queue to be sent.
    NetworkInstructionDeque instructions;
    {
        std::scoped_lock lock(m_eventsMutex);

        // Splice all network instruction events until the first non-instruction event
        // TODO: Should looping be flagged here? The endpoint could in theory be DOS'd by maniuplating
        // connect calls, how/why would this happen?
        while (m_events.size() != 0 && 
               m_events.front().type() == typeid(Tcp::TNetworkInstructionEvent)) {

            instructions.emplace_back(
                std::any_cast<Tcp::TNetworkInstructionEvent>(m_events.front()));

            m_events.pop_front();
        }
    }

    for (auto const& [type, address, port] : instructions) {
        IPv4SocketAddress socketAddress = {}; // Zero initialize the socket address;
        switch (type) {
            case NetworkInstruction::Bind: {
                // Set the provided listen descriptor to the new listening socket
                bool const success = Listen(listener, address, port, socketAddress);
                if (!success) {
                    std::string const notification = "[TCP] Binding to " + address + ":" + std::to_string(port) + " failed.";
                    NodeUtils::printo(notification, NodeUtils::PrintType::Endpoint);
                }
            } break;
            case NetworkInstruction::Connect: {
                bool const success = Connect(address, port, socketAddress);
                if (!success) {
                    std::string const notification = "[TCP] Connection to " + address + ":" + std::to_string(port) + " failed.";
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
void Endpoints::CTcpEndpoint::ProcessIncomingMessages()
{
    m_peers.ForEachConnection(
        [&](SocketDescriptor const& descriptor) -> CallbackIteration
        {
            OptionalReceiveResult optResult = Receive(descriptor);
            // If a request has been received, forward the message through the message sink and
            // wait until the message has been processed before accepting another message
            if (!optResult) {
                return CallbackIteration::Continue;
            }

            std::visit(TVariantVisitor
                {
                    [this, &descriptor](ConnectionStateChange change)
                    {
                        HandleConnectionStateChange(descriptor, change);
                    },
                    [this, &descriptor](Message::Buffer const& message) {
                        HandleReceivedData(descriptor, message);
                    },
                },
                *optResult);

            return CallbackIteration::Continue;
        }
    );
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Endpoints::CTcpEndpoint::OptionalReceiveResult Endpoints::CTcpEndpoint::Receive(
    SocketDescriptor descriptor)
{
    struct pollfd pfd = {};
	pfd.fd = descriptor;
	pfd.events = POLLIN | POLLHUP | POLLRDNORM;
	pfd.revents = 0;

    if (std::int32_t pollResult = ::poll(&pfd, 1, 0); pollResult > 0) {
        std::vector<std::uint8_t> buffer(ReadBufferSize, 0);
        std::int32_t received = ::recv(descriptor, buffer.data(), buffer.size(), MSG_DONTWAIT);
        switch (received) {
            case 0: return ConnectionStateChange::Disconnect;
            default: {
                if (received < 0) {
                    switch (errno) {
                        case EBADF: return ConnectionStateChange::Disconnect;
                        default: return {};
                    }
                }
                
                bool bMessageAllowed = true;
                [[maybe_unused]] bool const bPeerDetailsFound = m_peers.UpdateOnePeer(descriptor,
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



                buffer.resize(received);
                return buffer;
            }
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::HandleReceivedData(
    SocketDescriptor descriptor, Message::Buffer const& message)
{
    NodeUtils::printo("[TCP] Received request: " + std::string(message.begin(), message.end()), NodeUtils::PrintType::Endpoint);
    try {
        CMessage const request(message);
        auto const id = request.GetSourceId();
        // Update the information about the node as it pertains to received data. The node may not be found 
        // if this is its first connection.
        bool const bPeerDetailsFound = m_peers.UpdateOnePeer(descriptor,
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
            m_peers.PromoteConnection(descriptor, details);

            // Register this peer's message handler with the message sink
            m_messageSink->RegisterCallback(id, this, &CEndpoint::HandleProcessedMessage);
        }

        // TODO: Only authenticated requests should be forwarded into BryptNode
        m_messageSink->ForwardMessage(id, request);
    } catch (...) {
        NodeUtils::printo("[TCP] Received message failed to unpack.", NodeUtils::PrintType::Endpoint);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ProcessOutgoingMessages()
{
    // Splice elements up to send cycle limit into a temporary queue to be sent.
    OutgoingMessageDeque outgoing;
    {
        std::scoped_lock lock(m_eventsMutex);
        for (std::uint32_t idx = 0; idx < Endpoint::OutgoingMessageLimit; ++idx) {
            // If there are no messages left in the outgoing message queue, break from
            // copying the items into the temporary queue.
            if (m_events.size() == 0) {
                break;
            }

            // If we have reached an event that is not an outgoing message stop splicing
            auto const& eventType = m_events.front().type();
            if (eventType != typeid(Tcp::TOutgoingMessageEvent)) {
                break;
            }

            outgoing.emplace_back(
                std::any_cast<Tcp::TOutgoingMessageEvent>(m_events.front()));

            m_events.pop_front();
        }
    }

    for (auto const& [descriptor, message, retries] : outgoing) {

        // Determine if the message being sent is allowed given the current state of communications
        // with the peer. Brypt networking structure dictates that each request is coupled with a response.
        MessagingPhase phase = MessagingPhase::Response;
        m_peers.UpdateOnePeer(descriptor,
            [&phase] (auto& details) {
                phase = details.GetMessagingPhase();
            }
        );

        // If the current message is not allowed in the current network context, skip to the next message.
        if (phase != MessagingPhase::Response) {
            continue;
        }

        auto const result = Send(descriptor, message);  // Attempt to send the message
        std::visit(TVariantVisitor
            {
                [this, &descriptor](ConnectionStateChange change)
                {
                    HandleConnectionStateChange(descriptor, change);
                },
                [this, &descriptor, &message, &retries](std::uint32_t sent) {
                    // If the message was sent, update relevant peer tracking details
                    // Otherwise, schedule another send attempt if possible
                    if (sent != 0) {
                        m_peers.UpdateOnePeer(descriptor,
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
                            return;
                        }
                        std::uint8_t const attempts = retries + 1;
                        {
                            std::scoped_lock lock(m_eventsMutex);
                            m_events.emplace_back(
                                Tcp::TOutgoingMessageEvent(descriptor, message, attempts));
                        }
                    }
                },
            },
            result);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Endpoints::CTcpEndpoint::SendResult Endpoints::CTcpEndpoint::Send(
    SocketDescriptor descriptor, std::string_view message)
{
    std::int32_t result = ::send(descriptor, message.data(), message.size(), 0);
    if (result < 0) {
        switch (errno) {
            case EIO:
            case ENETDOWN:
            case ENETUNREACH:
            case ENOBUFS: {
                return 0;
            }
            default: {
                return ConnectionStateChange::Disconnect;
            }
        }
    }

    if (result == 0) {
        return result;
    }

    NodeUtils::printo("[TCP] Sent: " + std::string(message.data(), message.end()), NodeUtils::PrintType::Endpoint);

    return result;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::HandleConnectionStateChange(
    SocketDescriptor descriptor,
    [[maybe_unused]] ConnectionStateChange change)
{
    [[maybe_unused]] bool const bPeerDetailsFound = m_peers.UpdateOnePeer(descriptor,
        [&] (auto& details)
        {
            auto const id = details.GetNodeId();

            switch (details.GetConnectionState()) {
                case ConnectionState::Connected: {
                    details.SetConnectionState(ConnectionState::Disconnected);   
                    m_messageSink->UnpublishCallback(id);
                } break;
                // Other ConnectionStates are not currently handled for this endpoint
                default: assert(false); break;
            }
        }
    );
}

//------------------------------------------------------------------------------------------------