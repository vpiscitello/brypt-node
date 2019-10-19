//------------------------------------------------------------------------------------------------
// File: StreamBridgeConnection.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "StreamBridgeConnection.hpp"
#include "../../Utilities/Message.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

Connection::CStreamBridge::CStreamBridge()
    : CConnection()
    , m_control(false)
    , m_id()
    , m_port(std::string())
    , m_peerAddress(std::string())
    , m_peerPort(std::string())
    , m_initializationMessage(1)
    , m_context(nullptr)
    , m_socket()
{
}

//------------------------------------------------------------------------------------------------

Connection::CStreamBridge::CStreamBridge(NodeUtils::TOptions const& options)
    : CConnection(options)
    , m_control(options.m_isControl)
    , m_id()
    , m_port(options.m_port)
    , m_peerAddress(options.m_peerAddress)
    , m_peerPort(options.m_peerPort)
    , m_initializationMessage(1)
    , m_context(nullptr)
    , m_socket()
{
    printo("Creating StreamBridge instance", NodeUtils::PrintType::CONNECTION);

    m_updateClock = NodeUtils::GetSystemTimePoint();

    if (m_control) {
        printo("[StreamBridge] creating control socket", NodeUtils::PrintType::CONNECTION);

        m_context = std::make_unique<zmq::context_t>(1);

        switch (m_operation) {
            case NodeUtils::DeviceOperation::ROOT: {
               printo("[StreamBridge] Setting up StreamBridge socket on port " + options.m_port, NodeUtils::PrintType::CONNECTION);
               m_socket = std::make_unique<zmq::socket_t>(*m_context.get(), ZMQ_STREAM);
               m_initializationMessage = 1;
               SetupStreamBridgeSocket(options.m_port);
            } break;
            case NodeUtils::DeviceOperation::BRANCH:
            case NodeUtils::DeviceOperation::LEAF:
            case NodeUtils::DeviceOperation::NONE: break;
        }
    } else {
        Spawn();
        std::unique_lock<std::mutex> threadLock(m_workerMutex);
        this->m_workerConditional.wait(threadLock, [this]{return m_workerActive;});
        threadLock.unlock();
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
    printo("[StreamBridge] Spawning STREAMBRIDGE_TYPE connection thread", NodeUtils::PrintType::CONNECTION);
    m_workerThread = std::thread(&CStreamBridge::Worker, this);
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
NodeUtils::TechnologyType const& Connection::CStreamBridge::GetInternalType() const
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
    m_workerActive = true;
    CreatePipe();

    m_context = std::make_unique<zmq::context_t>(1);

    switch (m_operation) {
        case NodeUtils::DeviceOperation::ROOT: {
            printo("[StreamBridge] Setting up StreamBridge socket on port " + m_port, NodeUtils::PrintType::CONNECTION);
            m_socket = std::make_unique<zmq::socket_t>(*m_context.get(), ZMQ_STREAM);
            m_initializationMessage = 1;
            SetupStreamBridgeSocket(m_port);
        } break;
        case NodeUtils::DeviceOperation::BRANCH:
        case NodeUtils::DeviceOperation::LEAF:
        case NodeUtils::DeviceOperation::NONE: break;
    }

    // Notify the calling thread that the connection Worker is ready
    std::unique_lock<std::mutex> threadLock(m_workerMutex);
    m_workerConditional.notify_one();
    threadLock.unlock();

    std::optional<std::string> optRequest;
    std::string response;
    std::uint64_t run = 0;
    do {
        optRequest = Receive(0);
        if (optRequest) {
            WriteToPipe(*optRequest);

            threadLock.lock();
            this->m_workerConditional.wait(threadLock, [this]{return !m_responseNeeded;});
            threadLock.unlock();

            response = ReadFromPipe();
            Send(response.c_str());

            optRequest.reset();
            response.clear();
            }
        std::this_thread::sleep_for(std::chrono::nanoseconds(1000));
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
    m_instantiate = true;
    std::string connData = "tcp://*:" + port;
    zmq_bind(m_socket.get(), connData.c_str());
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
    zmq_recv(m_socket.get(), m_id, Connection::StreamBridge::ID_SIZE, flag);
    memset(buffer, '\0', Connection::StreamBridge::BUFFER_SIZE);

    zmq_recv(m_socket.get(), buffer, Connection::StreamBridge::BUFFER_SIZE, flag);
    memset(buffer, '\0', Connection::StreamBridge::BUFFER_SIZE);

    zmq_recv(m_socket.get(), buffer, Connection::StreamBridge::BUFFER_SIZE, flag);
    memset(buffer, '\0', Connection::StreamBridge::BUFFER_SIZE);

    zmq_recv(m_socket.get(), buffer, Connection::StreamBridge::BUFFER_SIZE, flag);
    printo("[StreamBridge] Received: " + std::string(buffer), NodeUtils::PrintType::CONNECTION);
    
    m_initializationMessage = 0;

    return buffer;
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
    zmq_send(m_socket.get(), m_id, Connection::StreamBridge::ID_SIZE, ZMQ_SNDMORE);
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
void Connection::CStreamBridge::Send(char const* const message)
{
    std::int32_t flag = 0;
    std::string toCat = std::string();
    if ((message)[strlen(message)] != static_cast<std::int8_t>(4)) {
        flag = 1;
        toCat = std::to_string(static_cast<std::int8_t>(4));
    }
    zmq_send(m_socket.get(), m_id, Connection::StreamBridge::ID_SIZE, ZMQ_SNDMORE);
    zmq_send(m_socket.get(), message, strlen(message), ZMQ_SNDMORE);
    if (flag) {
        zmq_send(m_socket.get(), toCat.c_str(), strlen(toCat.c_str()), ZMQ_SNDMORE);
    }
    printo("[StreamBridge] Sent: (" + std::to_string(strlen(message)) + ") " + message, NodeUtils::PrintType::CONNECTION);
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
void Connection::CStreamBridge::Shutdown()
{
    printo("[StreamBridge] Shutting down socket and context", NodeUtils::PrintType::CONNECTION);
    zmq_close(m_socket.release());
    zmq_ctx_destroy(m_context.release());
}


//------------------------------------------------------------------------------------------------
