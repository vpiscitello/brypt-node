//------------------------------------------------------------------------------------------------
// File: DirectConnection.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "DirectConnection.hpp"
#include "../../Utilities/Message.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------
constexpr std::chrono::nanoseconds timeout = std::chrono::nanoseconds(1000);
//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

Connection::CDirect::CDirect(
    IMessageSink* const messageSink,
    Configuration::TConnectionOptions const& options)
    : CConnection(messageSink, options)
    , m_port()
    , m_peerAddress()
    , m_peerPort()
    , m_context(nullptr)
    , m_socket(nullptr)
    , m_pollItem()
    , m_bProcessReceived(false)
{
    printo("[Direct] Creating direct instance", NodeUtils::PrintType::CONNECTION);

    // Get the two networking components from the supplied options
    auto const bindingComponents = options.GetBindingComponents();
    auto const peerComponents = options.GetEntryComponents();

    {
        std::scoped_lock lock(m_mutex);

        m_port = bindingComponents.second; // The second binding component is the port we intend on binding on
        m_peerAddress = peerComponents.first; // The first peer component is the IP address of the peer
        m_peerPort = peerComponents.second; // The second peer component is the port on the peer
        m_context = std::make_unique<zmq::context_t>(1); // Setup the ZMQ context we are operating on
        
        // Register this connections processed message handler with the message sink
        m_messageSink->RegisterCallback(m_id, this, &CConnection::HandleProcessedMessage);
    }

    Spawn(); // Spawn a new worker thread that will handle message receiving
}

//------------------------------------------------------------------------------------------------

Connection::CDirect::~CDirect()
{
    bool const success = Shutdown(); // Attempt to shutdown the worker thread
    if (!success) {
        m_worker.detach(); // If the thread could not be shutdown for some reason, stop managing it.
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::whatami()
{
    printo("[Direct] I am a Direct implementation", NodeUtils::PrintType::CONNECTION);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::string const& Connection::CDirect::GetProtocolType() const
{
    static std::string const protocol = "IEEE 802.11";  // Not true, it is possible to use ethernet for example
    return protocol;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType Connection::CDirect::GetInternalType() const
{
    static NodeUtils::TechnologyType const internal = NodeUtils::TechnologyType::DIRECT;
    return internal;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::Spawn()
{
    printo("[Direct] Spawning DIRECT connection thread", NodeUtils::PrintType::CONNECTION);
    // Depending upon the intended operation of this connection we need to setup the ZMQ sockets
    // in either Request or Reply modes. This changes the pattern of how the socket is intented
    // to be used.
    switch (m_operation) {
        // If the intended operation is to act as a server connection, we expect requests to be 
        // received first to which we process the message then reply.
        case NodeUtils::ConnectionOperation::SERVER: {
            printo("[Direct] Setting up REP socket on port " + m_port, NodeUtils::PrintType::CONNECTION);
            SetupServerWorker(m_port);
        } break;
        // If the intended operation is to act as client connection, we expect to send requests
        // from which we will receive a reply.
        case NodeUtils::ConnectionOperation::CLIENT: {
            printo("[Direct] Connecting REQ socket to " + m_peerAddress, NodeUtils::PrintType::CONNECTION);
            SetupClientWorker(m_peerAddress, m_peerPort);
        } break;
        default: break;
    }

    // Wait for the spawned thread to signal it is ready to be used
    std::unique_lock lock(m_mutex);
    this->m_cv.wait(lock, [this]{ return m_active.load(); });
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::Worker()
{
 
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::HandleProcessedMessage(std::string_view message)
{
    Send(message); // Forward the received message to be sent on the socket
    m_cv.notify_all(); // Notify that the message has been sent
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::Send(CMessage const& message)
{
    Send(message.GetPack()); // Forward the message pack to be sent on the socket
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::Send(std::string_view message)
{
    // If the message provided is empty, do not send anything
    if (message.empty()) {
        return;
    }

    // Create a zmq message from the provided data and send it
    zmq::message_t request(message.data(), message.size());
    m_socket->send(request, zmq::send_flags::none);

    printo(
        "[Direct] Sent: (" + std::to_string(message.size()) + ") " 
        + message.data(), NodeUtils::PrintType::CONNECTION);

    ++m_sequenceNumber;

    m_bProcessReceived = !m_bProcessReceived;
    m_cv.notify_all();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::optional<std::string> Connection::CDirect::Receive(std::int32_t flag)
{
    // Interpret the provided flag as a zmq receive flag and then receive
    zmq::recv_flags zmqFlag = static_cast<zmq::recv_flags>(flag);
    return Receive(zmqFlag);
;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::PrepareForNext()
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Connection::CDirect::Shutdown()
{
    printo("[Direct] Shutting down socket and context", NodeUtils::PrintType::CONNECTION);
    // Stop the worker thread from processing the connections
    m_terminate = true;

    m_cv.notify_all(); // Notify the worker that exit conditions have been set
    
    if (m_worker.joinable()) {
        m_worker.join();
    }

    {
        std::scoped_lock lock(m_mutex);
        if (m_socket->connected()) {
            m_socket->close();
        }
        m_context->close();
    }
    
    return !m_worker.joinable();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::SetupServerWorker(
    NodeUtils::PortNumber const& port)
{
    std::scoped_lock lock(m_mutex);
    if(!m_context) {
        std::runtime_error("ZMQ Context has not been initialized!");
    }
    m_socket = std::make_unique<zmq::socket_t>(*m_context.get(), ZMQ_REP);
    m_pollItem.socket = m_socket.get();
    m_pollItem.events = ZMQ_POLLIN;
    m_socket->bind("tcp://*:" + port);
    
    m_worker = std::thread(&CDirect::ServerWorker, this); // Create the work thread 
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::SetupClientWorker(
    NodeUtils::NetworkAddress const& address,
    NodeUtils::PortNumber const& port)
{
    std::scoped_lock lock(m_mutex);
    if (!m_context) {
        std::runtime_error("ZMQ Context has not been initialized!");
    } 
    m_socket = std::make_unique<zmq::socket_t>(*m_context.get(), ZMQ_REQ);
    m_pollItem.socket = m_socket.get();
    m_pollItem.events = ZMQ_POLLIN;
    m_socket->connect("tcp://" + address + ":" + port);

    m_worker = std::thread(&CDirect::ClientWorker, this); // Create the work thread 
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::ServerWorker()
{
    // Notify the calling thread that the connection worker is ready
    m_active = true;
    m_cv.notify_all();

    do {
        // Attempt to receive a request on our socket while not blocking.
        auto const request = Receive(zmq::recv_flags::dontwait);

        // If a request has been received, forward the message through the message sink and
        // wait until the message has been processed before accepting another message
        if (request) {
            printo("[Direct] Received request: " + *request, NodeUtils::PrintType::CONNECTION);
            std::unique_lock lock(m_mutex);
            m_messageSink->ForwardMessage(m_id, *request);
            m_bProcessReceived = !m_bProcessReceived;
            m_cv.notify_all();

            // Wait for message from pipe then Send
            if (!m_bProcessReceived) {
                this->m_cv.wait(lock, [this]{return m_bProcessReceived || m_terminate;});
            }
        }

        if (m_terminate) {
            return;
        }
        
        // Gracefully handle thread termination by waiting a period of time for a terminate 
        // signal before continuing normal operation. 
        {
            std::unique_lock lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + local::timeout;
            m_cv.wait_until(lock, stop, [&]{ return m_terminate.load(); });
            if (m_terminate) {
                return; // Terminate thread if the timeout was not reached
            }
        }
    } while(true);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::ClientWorker()
{
    // Notify the calling thread that the connection worker is ready
    m_active = true;
    m_cv.notify_all();

    do {
        if (!m_bProcessReceived) {
            std::unique_lock lock(m_mutex);
            this->m_cv.wait(lock, [this]{return m_bProcessReceived || m_terminate;});
        }

        if (m_terminate) {
            return;
        }

        // Attempt to receive a request on our socket while not blocking.
        auto const request = Receive(zmq::recv_flags::dontwait);

        // If a request has been received, forward the message through the message sink and
        // wait until the message has been processed before accepting another message
        if (request) {
            printo("[Direct] Received request: " + *request, NodeUtils::PrintType::CONNECTION);
            m_messageSink->ForwardMessage(m_id, *request);
            m_bProcessReceived = !m_bProcessReceived;
            m_cv.notify_all();
        }
        
        // Gracefully handle thread termination by waiting a period of time for a terminate 
        // signal before continuing normal operation. 
        {
            std::unique_lock lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + local::timeout;
            m_cv.wait_until(lock, stop, [&]{ return m_terminate.load(); });
            if (m_terminate) {
                return; // Terminate thread if the timeout was not reached
            }
        }
    } while(true);
}

//------------------------------------------------------------------------------------------------

std::optional<std::string> Connection::CDirect::Receive(zmq::recv_flags flag)
{
    zmq::message_t message;
    auto const result = m_socket->recv(message, flag);
    if (!result || *result == 0) {
        return {};
    }

    std::string request = std::string(static_cast<char*>(message.data()), *result);
    printo("[Direct] Received: " + request, NodeUtils::PrintType::CONNECTION);

    m_updateTimePoint = NodeUtils::GetSystemTimePoint();
    ++m_sequenceNumber;

    m_bProcessReceived = !m_bProcessReceived;
    m_cv.notify_all();

    return request;
}

//------------------------------------------------------------------------------------------------
