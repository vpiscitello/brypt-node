//------------------------------------------------------------------------------------------------
// File: TcpEndpoint.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "TcpEndpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "EndpointDefinitions.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../Command/CommandDefinitions.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/MessageUtils.hpp"
#include "../../BryptMessage/PackUtils.hpp"
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
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    std::string_view interface,
    OperationType operation,
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator)
    : CEndpoint(
        spBryptIdentifier, interface, operation,
        pEndpointMediator, pPeerMediator, TechnologyType::TCP)
    , m_address()
    , m_port(0)
    , m_tracker()
    , m_eventsMutex()
    , m_events()
    , m_scheduler()
{
    m_scheduler = [this] (
        BryptIdentifier::CContainer const& destination, std::string_view message) -> bool
        {
            return ScheduleSend(destination, message);
        };
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
Endpoints::TechnologyType Endpoints::CTcpEndpoint::GetInternalType() const
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
    std::string entry = "";
    if (m_operation == OperationType::Client || m_address.empty()) {
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
std::string Endpoints::CTcpEndpoint::GetURI() const
{
    return Scheme.data() + GetEntry();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::ScheduleBind(std::string_view binding)
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
            Tcp::TNetworkInstructionEvent(NetworkInstruction::Bind, address, port));
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
void Endpoints::CTcpEndpoint::ScheduleConnect(std::string_view entry)
{
    if (m_operation != OperationType::Client) {
        throw std::runtime_error("Connect was called a non-client Endpoint!");
    }
    
    auto const [address, sPort] = NetworkUtils::SplitAddressString(entry);
    if (address.empty() || sPort.empty()) {
        return;
    }

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
bool  Endpoints::CTcpEndpoint::ScheduleSend(
    BryptIdentifier::CContainer const& identifier,
    std::string_view message)
{
    // If the message provided is empty, do not send anything
    if (message.empty()) {
        return false;
    }

    // Get the socket descriptor of the intended destination. If it is not found, drop the message.
    auto const optDescriptor = m_tracker.Translate(identifier);
    if (!optDescriptor) {
        return false;
    }

    // Schedule the outgoing message event
    {
        std::scoped_lock lock(m_eventsMutex);
        m_events.emplace_back(
            Tcp::TOutgoingMessageEvent(*optDescriptor, message, 0));
    }

    return true;
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

    m_tracker.ReadEachConnection(
        [&](auto const& descriptor, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            ::close(descriptor); // Close the connection descriptor
            return CallbackIteration::Continue;
        }
    );

    m_tracker.Clear();
    
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
        case OperationType::Server: {
            bWorkerStarted = SetupServerWorker(); 
        } break;
        case OperationType::Client: {
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
    SocketDescriptor listener = Tcp::InvalidDescriptor;
    
    // Notify the calling thread that the connection worker is ready
    m_active = true;
    m_cv.notify_all();

    // Start the endpoint's event loop 
    while(m_active) {
        ProcessNetworkInstructions(&listener); // Process any endpoint instructions at the front if the event queue
        AcceptConnection(listener);
        ProcessIncomingMessages(); // Process any messages on the socket to be handled this cycle 
        ProcessOutgoingMessages(); // Send messages from the outgoing message queue
        
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

    ::close(listener);  // Close the server thread's local listen socket
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
    std::string uri;
    uri.append(Scheme.data());
    uri.append(address);
    uri.append(NetworkUtils::ComponentSeperator);
    uri.append(std::to_string(port));

    NodeUtils::printo("[TCP] Setting up TCP socket on " + uri, NodeUtils::PrintType::Endpoint);

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

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Endpoints::CTcpEndpoint::AcceptConnection(SocketDescriptor listener)
{
    // Create a static socket address, so we don't need to create it everytime we check the socket 
    static IPv4SocketAddress address = {}; // Zero initialize the socket address
    socklen_t size = SocketAddressSize;

    // Accept a connection that may be queued on the socket
    SocketDescriptor connection = ::accept(
        listener,
        reinterpret_cast<sockaddr*>(&address),
        &size);

    // If an invalid descriptor has been returned by accept then return
    if (connection < 0) {
        return;
    }

    // Make the accepted socket non-blocking
    if (::fcntl(connection, F_SETFL, ::fcntl(connection, F_GETFL) | O_NONBLOCK) < 0) {
        return;
    }
    
    m_tracker.TrackConnection(connection);

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
            auto const stop = std::chrono::system_clock::now() + Endpoints::CycleTimeout;
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
Endpoints::CTcpEndpoint::ConnectStatusCode Endpoints::CTcpEndpoint::Connect(
    NetworkUtils::NetworkAddress const& address,
    NetworkUtils::PortNumber port,
    IPv4SocketAddress& socketAddress)
{
    std::string uri;
    uri.append(Scheme.data());
    uri.append(address);
    uri.append(NetworkUtils::ComponentSeperator);
    uri.append(std::to_string(port));
    if (auto const status = IsURIAllowed(uri); status != ConnectStatusCode::Success) {
        return status;
    }

    NodeUtils::printo("[TCP] Connecting TCP socket to " + uri, NodeUtils::PrintType::Endpoint);

    socketAddress.sin_family = AF_INET;
    socketAddress.sin_port = htons(port);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (::inet_pton(AF_INET, address.c_str(), &socketAddress.sin_addr) <= 0) {
        return ConnectStatusCode::GenericError;
    }

    SocketDescriptor descriptor = std::numeric_limits<std::int32_t>::min();
    if (descriptor = ::socket(AF_INET, SOCK_STREAM, 0); descriptor < 0) {
        return ConnectStatusCode::GenericError;
    }

    if (m_pPeerMediator) {
        return ConnectStatusCode::GenericError;
    }

    // Get the connection request message from the peer mediator. If we have not been given 
    // an expected identifier declare a new peer using the peer's URI. Otherwise, declare 
    // the resolving peer using the expected identifier. If the peer is known through the 
    // identifier, the mediator will provide us a short circuit handshake request.
    std::optional<std::string> optConnectionRequest = m_pPeerMediator->DeclareResolvingPeer(uri);

    if (!optConnectionRequest) {
        return ConnectStatusCode::GenericError;
    }

    if (auto const status = EstablishConnection(descriptor, socketAddress, *optConnectionRequest);
        status != ConnectStatusCode::Success) {
        return status;
    }

    m_tracker.TrackConnection(descriptor, uri);  // Start tracking the descriptor for communication

    return ConnectStatusCode::Success;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Endpoints::CTcpEndpoint::ConnectStatusCode Endpoints::CTcpEndpoint::IsURIAllowed(std::string_view uri)
{
    // Determine if the provided URI matches any of the node's hosted entrypoints.
    bool bUriMatchedEntry = false;
    if (m_pEndpointMediator) {
        auto const entries = m_pEndpointMediator->GetEndpointEntries();
        for (auto const& [technology, entry]: entries) {
            if (auto const pos = uri.find(entry); pos != std::string::npos) {
                bUriMatchedEntry = true; 
                break;
            }
        }
    }
    // If the URI matched an entrypoint, the connection should not be allowed as
    // it would be a connection to oneself.
    if (bUriMatchedEntry) {
        return ConnectStatusCode::ReflectionError;
    }

    // If the URI matches a currently connected or resolving peer. The connection
    // should not be allowed as it break a valid connection. 
    if (bool const bWouldBreakConnection = m_tracker.IsURITracked(uri); bWouldBreakConnection) {
        return ConnectStatusCode::DuplicateError;
    }

    return ConnectStatusCode::Success;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Endpoints::CTcpEndpoint::ConnectStatusCode Endpoints::CTcpEndpoint::EstablishConnection(
    SocketDescriptor descriptor, IPv4SocketAddress address, std::string_view request)
{
    std::uint32_t attempts = 0;
    socklen_t size = SocketAddressSize;
    while (::connect(descriptor, reinterpret_cast<sockaddr *>(&address), size) < 0) {
        std::string error = "Unspecifed error";
        if (char const* const result = strerror(errno); result != nullptr) {
            error = std::string(result, strlen(result));
        } 

        NodeUtils::printo(
            "[TCP] Connection to peer failed. Error: " + error, NodeUtils::PrintType::Endpoint);

        if (++attempts > Endpoints::ConnectRetryThreshold) {
            return ConnectStatusCode::RetryError;
        }

        {
            std::unique_lock lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + Endpoints::ConnectRetryTimeout;
            m_cv.wait_until(lock, stop, [&]{ return m_terminate.load(); });
            if (m_terminate) {
                return ConnectStatusCode::Terminated; // Terminate if the endpoint is shutting down
            }
        }
    }

    auto const result = Send(descriptor, request);
    if (auto const pValue = std::get_if<std::int32_t>(&result); pValue == nullptr || *pValue <= 0) {
        return ConnectStatusCode::GenericError;
    }

    return ConnectStatusCode::Success;
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
                    std::string const notification = "[TCP] Binding to " + address + ":"\
                         + std::to_string(port) + " failed.";
                    NodeUtils::printo(notification, NodeUtils::PrintType::Endpoint);
                }
            } break;
            case NetworkInstruction::Connect: {
                auto const status = Connect(address, port, socketAddress);
                if (status == ConnectStatusCode::GenericError) {
                    std::string const notification = "[TCP] Connection to " + address + ":"\
                         + std::to_string(port) + " failed.";
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
    m_tracker.ForEachConnection(
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
                    [this, &descriptor](Message::Buffer const& buffer) {
                        HandleReceivedData(descriptor, buffer);
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
                [[maybe_unused]] bool const bConnectionDetailsFound = m_tracker.UpdateOneConnection(descriptor,
                    [&bMessageAllowed](auto& details) {
                        // TODO: A generic message filtering service should be used to ensure all connection interfaces
                        // use the same message allowance scheme. 
                        MessagingPhase phase = details.GetMessagingPhase();
                        if (phase != MessagingPhase::Request) {
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
    SocketDescriptor descriptor, Message::Buffer const& buffer)
{
    NodeUtils::printo(
        "[TCP] Received message: " + std::string(buffer.begin(), buffer.end()),
        NodeUtils::PrintType::Endpoint);

    auto const pBuffer = reinterpret_cast<char const*>(buffer.data());
    auto const decoded = PackUtils::Z85Decode({ pBuffer, buffer.size() });
    auto const optIdentifier = Message::PeekSource(decoded.begin(), decoded.end());
    if (!optIdentifier) {
        return;
    }

    // Update the information about the node as it pertains to received data. The node may not be
    // found if this is its first connection.
    std::shared_ptr<CBryptPeer> spBryptPeer = {};
    m_tracker.UpdateOneConnection(descriptor,
        [&spBryptPeer] (auto& details) {
            spBryptPeer = details.GetBryptPeer();
            details.IncrementMessageSequence();
        },
        [this, &optIdentifier, &spBryptPeer] (std::string_view uri) -> ExtendedConnectionDetails
        {
            spBryptPeer = CEndpoint::LinkPeer(*optIdentifier);
            
            ExtendedConnectionDetails details(spBryptPeer);
            details.SetConnectionState(ConnectionState::Connected);
            details.SetMessagingPhase(MessagingPhase::Response);
            details.IncrementMessageSequence();
            
            spBryptPeer->RegisterEndpoint(m_identifier, m_technology, m_scheduler, uri);

            return details;
        }
    );

    spBryptPeer->ScheduleReceive({ m_identifier, m_technology }, decoded);
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
        for (std::uint32_t idx = 0; idx < Endpoints::OutgoingMessageLimit; ++idx) {
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
        m_tracker.ReadOneConnection(descriptor,
            [&phase] (auto const& details) {
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
                        m_tracker.UpdateOneConnection(descriptor,
                            [] (auto& details) {
                                details.IncrementMessageSequence();
                                details.SetMessagingPhase(MessagingPhase::Request);
                            }
                        );
                    } else {
                        // If we have already attempted to send the message three times, drop the message.
                        if (retries == Endpoints::MessageRetryLimit) {
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
    ConnectionStateChange change)
{
    [[maybe_unused]] bool const bConnectionDetailsFound = m_tracker.UpdateOneConnection(descriptor,
        [&] (auto& details)
        {
            auto const spBryptPeer = details.GetBryptPeer();
            
            switch (details.GetConnectionState()) {
                case ConnectionState::Connected: {
                    details.SetConnectionState(ConnectionState::Disconnected);
                    if (spBryptPeer) {
                        spBryptPeer->WithdrawEndpoint(m_identifier, m_technology);
                    }
                } break;
                default: break;
            }
        }
    );

    switch (change) {
        case ConnectionStateChange::Disconnect: {
            m_tracker.UntrackConnection(descriptor);
        } break;
        default: break;
    }
}

//------------------------------------------------------------------------------------------------