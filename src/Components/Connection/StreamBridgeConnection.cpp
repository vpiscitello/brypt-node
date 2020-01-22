//------------------------------------------------------------------------------------------------
// File: StreamBridgeConnection.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "StreamBridgeConnection.hpp"
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

Connection::CStreamBridge::CStreamBridge(
    IMessageSink* const messageSink,
    Configuration::TConnectionOptions const& options)
    : CConnection(messageSink, options)
    , m_streamId()
    , m_port()
    , m_peerAddress()
    , m_peerPort()
    , m_initializationMessage(1)
    , m_context(nullptr)
    , m_socket()
{
    printo("Creating StreamBridge instance", NodeUtils::PrintType::CONNECTION);
    
    auto const bindingComponents = options.GetBindingComponents();
    auto const peerComponents = options.GetEntryComponents();

    m_port = bindingComponents.second;
    m_peerAddress = peerComponents.first;
    m_peerPort = peerComponents.second;

    m_updateTimePoint = NodeUtils::GetSystemTimePoint();

    Spawn();
    std::unique_lock lock(m_mutex);
    this->m_cv.wait(lock, [this]{return m_active.load();});
    lock.unlock();
}

//------------------------------------------------------------------------------------------------

Connection::CStreamBridge::~CStreamBridge()
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
void Connection::CStreamBridge::whatami()
{
    printo("[StreamBridge] I am a StreamBridge implementation", NodeUtils::PrintType::CONNECTION);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CStreamBridge::Spawn()
{
    printo("[StreamBridge] Spawning STREAMBRIDGE connection thread", NodeUtils::PrintType::CONNECTION);
    m_worker = std::thread(&CStreamBridge::Worker, this);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
 // Description:
 //------------------------------------------------------------------------------------------------
std::string const& Connection::CStreamBridge::GetProtocolType() const
{
    static std::string const protocol = "IEEE 802.11";  // Not true, it is possible to use ethernet for example
    return protocol;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
 // Description:
 //------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType Connection::CStreamBridge::GetInternalType() const
{
    static NodeUtils::TechnologyType const internal = NodeUtils::TechnologyType::STREAMBRIDGE;
    return internal;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CStreamBridge::Worker()
{
    m_active = true;

    m_context = std::make_unique<zmq::context_t>(1);

    switch (m_operation) {
        case NodeUtils::ConnectionOperation::SERVER: {
            printo("[StreamBridge] Setting up StreamBridge socket on port " + m_port, NodeUtils::PrintType::CONNECTION);
            m_socket = std::make_unique<zmq::socket_t>(*m_context.get(), ZMQ_STREAM);
            m_initializationMessage = 1;
            SetupStreamBridgeSocket(m_port);
        } break;
        default: break;
    }

    // Notify the calling thread that the connection Worker is ready
    m_cv.notify_one();

    std::optional<std::string> optRequest;
    std::uint64_t run = 0;
    do {
        optRequest = Receive(0);
        if (optRequest) {
            std::unique_lock lock(m_mutex);
            m_messageSink->ForwardMessage(m_id, *optRequest);
            
            optRequest.reset();
        }

        {
            std::unique_lock lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + local::timeout;
            auto const terminate = m_cv.wait_until(lock, stop, [&]{ return m_terminate.load(); });
            if (terminate) {
                return; // Terminate thread if the timeout was not reached
            }
        }
        ++run;
    } while(true);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CStreamBridge::SetupStreamBridgeSocket(NodeUtils::PortNumber const& port)
{
    printo("[StreamBridge] Setting up StreamBridge socket on port " + m_port, NodeUtils::PrintType::CONNECTION);
    std::string connData = "tcp://*:" + port;
    zmq_bind(m_socket.get(), connData.c_str());
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CStreamBridge::HandleProcessedMessage(std::string_view message)
{
    std::scoped_lock lock(m_mutex);
    Send(message);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CStreamBridge::Send(CMessage const& message)
{
    std::int32_t flag = 0;
    std::string toCat = std::string();
    std::string pack = message.GetPack();
    zmq_send(m_socket.get(), m_streamId, Connection::StreamBridge::ID_SIZE, ZMQ_SNDMORE);
    if ((pack.c_str())[strlen(pack.c_str())] != static_cast<std::int8_t>(4)) {
        flag = 1;
        toCat = std::to_string(static_cast<std::int8_t>(4));
    }
    zmq_send(m_socket.get(), pack.c_str(), strlen(pack.c_str()), ZMQ_SNDMORE);
    if (flag) {
        zmq_send(m_socket.get(), toCat.c_str(), strlen(toCat.c_str()), ZMQ_SNDMORE);
    }
    printo("[StreamBridge] Sent: (" + std::to_string(strlen(pack.c_str())) + ") " + pack, NodeUtils::PrintType::CONNECTION);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CStreamBridge::Send(std::string_view message)
{
    std::int32_t flag = 0;
    std::string toCat = std::string();
    if ((message)[message.size()] != static_cast<std::int8_t>(4)) {
        flag = 1;
        toCat = std::to_string(static_cast<std::int8_t>(4));
    }
    zmq_send(m_socket.get(), m_streamId, Connection::StreamBridge::ID_SIZE, ZMQ_SNDMORE);
    zmq_send(m_socket.get(), message.data(), message.size(), ZMQ_SNDMORE);
    if (flag) {
        zmq_send(m_socket.get(), toCat.c_str(), strlen(toCat.c_str()), ZMQ_SNDMORE);
    }
    printo("[StreamBridge] Sent: (" + std::to_string(message.size()) + ") " + message.data(), NodeUtils::PrintType::CONNECTION);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
 // Description:
 // Returns:
 //------------------------------------------------------------------------------------------------
std::optional<std::string> Connection::CStreamBridge::Receive(std::int32_t flag)
{
    char buffer[Connection::StreamBridge::BUFFER_SIZE];
    memset(buffer, '\0', Connection::StreamBridge::BUFFER_SIZE);

    // Receive 4 times, first is ID, second is nothing, third is message
    zmq_recv(m_socket.get(), m_streamId, Connection::StreamBridge::ID_SIZE, flag);
    memset(buffer, '\0', Connection::StreamBridge::BUFFER_SIZE);

    zmq_recv(m_socket.get(), buffer, Connection::StreamBridge::BUFFER_SIZE, flag);
    memset(buffer, '\0', Connection::StreamBridge::BUFFER_SIZE);

    zmq_recv(m_socket.get(), buffer, Connection::StreamBridge::BUFFER_SIZE, flag);
    memset(buffer, '\0', Connection::StreamBridge::BUFFER_SIZE);
    
    if (strlen(buffer) == 0) {
        return {};
    }

    zmq_recv(m_socket.get(), buffer, Connection::StreamBridge::BUFFER_SIZE, flag);
    printo("[StreamBridge] Received: " + std::string(buffer), NodeUtils::PrintType::CONNECTION);
    
    m_initializationMessage = 0;

    return buffer;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CStreamBridge::PrepareForNext()
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Connection::CStreamBridge::Shutdown()
{
    printo("[StreamBridge] Shutting down socket and context", NodeUtils::PrintType::CONNECTION);
    // Stop the worker thread from processing the connections
    {
        std::scoped_lock lock(m_mutex);
        zmq_close(m_socket.release());
        zmq_ctx_destroy(m_context.release());
        m_terminate = true;
    }

    m_cv.notify_all();
    
    if (m_worker.joinable()) {
        m_worker.join();
    }
    
    return !m_worker.joinable();
}


//------------------------------------------------------------------------------------------------
