//------------------------------------------------------------------------------------------------
// File: DirectConnection.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "DirectConnection.hpp"
#include "../../Utilities/Message.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

Connection::CDirect::CDirect()
    : CConnection()
    , m_control(false)
    , m_port(std::string())
    , m_peerAddress(std::string())
    , m_peerPort(std::string())
    , m_context(nullptr)
    , m_socket(nullptr)
    , m_pollItem()
{
}

//------------------------------------------------------------------------------------------------

Connection::CDirect::CDirect(NodeUtils::TOptions const& options)
    : CConnection(options)
    , m_control(options.m_isControl)
    , m_port(options.m_port)
    , m_peerAddress(options.m_peerAddress)
    , m_peerPort(options.m_peerPort)
    , m_context(nullptr)
    , m_socket(nullptr)
    , m_pollItem()

{
    printo("[Direct] Creating direct instance", NodeUtils::PrintType::CONNECTION);

    m_context = std::make_unique<zmq::context_t>(1);

    if (m_control) {
        printo("[Direct] Creating control socket", NodeUtils::PrintType::CONNECTION);
        switch (m_operation) {
            case NodeUtils::DeviceOperation::ROOT: {
                printo("[Direct] Setting up REP socket on port " + m_port, NodeUtils::PrintType::CONNECTION);
                SetupResponseSocket(options.m_port);
            } break;
            case NodeUtils::DeviceOperation::BRANCH: break;
            case NodeUtils::DeviceOperation::LEAF: {
                printo("[Direct] Connecting REQ socket to " + m_peerAddress, NodeUtils::PrintType::CONNECTION);
                SetupRequestSocket(m_peerAddress, m_peerPort);
            } break;
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
NodeUtils::TechnologyType const& Connection::CDirect::GetInternalType() const
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
    printo("[Direct] Spawning DIRECT_TYPE connection thread", NodeUtils::PrintType::CONNECTION);
    m_workerThread = std::thread(&CDirect::Worker, this);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::Worker()
{
    m_workerActive = true;
    CreatePipe();

    switch (m_operation) {
        case NodeUtils::DeviceOperation::ROOT: {
            printo("[Direct] Setting up REP socket on port " + m_port, NodeUtils::PrintType::CONNECTION);
            SetupResponseSocket(m_port);
        } break;
        case NodeUtils::DeviceOperation::BRANCH: break;
        case NodeUtils::DeviceOperation::LEAF: {
            printo("[Direct] Connecting REQ socket to " + m_peerAddress, NodeUtils::PrintType::CONNECTION);
            SetupRequestSocket(m_peerAddress, m_peerPort);
        } break;
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
        // Receive message
        optRequest = Receive(0);
        if (optRequest) {
            printo("[Direct] Received request: " + *optRequest, NodeUtils::PrintType::CONNECTION);
            WriteToPipe(*optRequest);
            m_responseNeeded = true;

            // Wait for message from pipe then Send
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
void Connection::CDirect::SetupResponseSocket(NodeUtils::PortNumber const& port)
{
    if(!m_context) {
        std::runtime_error("ZMQ Context has not been initialized!");
    }
    m_socket = std::make_unique<zmq::socket_t>(*m_context.get(), ZMQ_REP);
    m_pollItem.socket = m_socket.get();
    m_pollItem.events = ZMQ_POLLIN;
    m_instantiate = true;
    m_socket->bind("tcp://*:" + port);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::SetupRequestSocket(NodeUtils::IPv4Address const& address, NodeUtils::PortNumber const& port)
{
    if (!m_context) {
        std::runtime_error("ZMQ Context has not been initialized!");
    } 
    m_socket = std::make_unique<zmq::socket_t>(*m_context.get(), ZMQ_REQ);
    m_pollItem.socket = m_socket.get();
    m_pollItem.events = ZMQ_POLLIN;
    m_socket->connect("tcp://" + address + ":" + port);
    m_instantiate = false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::Send(CMessage const& message)
{
    if (!m_context) {
        std::runtime_error("ZMQ Context has not been initialized!");
    } 
    std::string const& pack = message.GetPack();
    zmq::message_t request(pack.size());
    memcpy(&request, pack.c_str(), pack.size());

    m_socket->send(request, zmq::send_flags::none);
    printo("[Direct] Sent: (" + std::to_string(strlen(pack.c_str())) + ") " + pack, NodeUtils::PrintType::CONNECTION);

    ++m_sequenceNumber;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CDirect::Send(char const* const message)
{
    if (!m_context) {
        std::runtime_error("ZMQ Context has not been initialized!");
    } 
    zmq::message_t request(strlen(message));
    memcpy(&request, message, strlen(message));

    m_socket->send(request, zmq::send_flags::none);
    printo("[Direct] Sent: (" + std::to_string(strlen(message)) + ") " + message, NodeUtils::PrintType::CONNECTION);

    ++m_sequenceNumber;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::optional<std::string> Connection::CDirect::Receive(std::int32_t flag)
{
    if (!m_context) {
        std::runtime_error("ZMQ Context has not been initialized!");
    }
    zmq::recv_flags zmqFlag = static_cast<zmq::recv_flags>(flag);
    zmq::message_t message;
    m_socket->recv(message, zmqFlag);
    std::string request = std::string(static_cast<char*>(message.data()), message.size());

    if (request.empty()) {
        return {};
    }

    m_updateClock = NodeUtils::GetSystemTimePoint();
    ++m_sequenceNumber;

    printo("[Direct] Received: " + request, NodeUtils::PrintType::CONNECTION);
    return request;
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
void Connection::CDirect::Shutdown()
{
    printo("[Direct] Shutting down socket and context", NodeUtils::PrintType::CONNECTION);
    zmq_close(m_socket.release());
    zmq_ctx_destroy(m_context.release());
}

//------------------------------------------------------------------------------------------------
