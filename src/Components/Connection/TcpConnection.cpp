//------------------------------------------------------------------------------------------------
// File: TcpConnection.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "TcpConnection.hpp"
#include "../../Utilities/Message.hpp"
//------------------------------------------------------------------------------------------------
#include <cerrno>
#include <chrono>
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

Connection::CTcp::CTcp()
    : CConnection()
    , m_port(std::string())
    , m_peerAddress(std::string())
    , m_peerPort(std::string())
    , m_socket(0)
    , m_connection(-1)
    , m_address()
{
    memset(&m_address, 0, sizeof(m_address));
}

//------------------------------------------------------------------------------------------------

Connection::CTcp::CTcp(Configuration::TConnectionOptions const& options)
    : CConnection(options)
    , m_port(options.binding)
    , m_peerAddress()
    , m_peerPort()
    , m_socket(0)
    , m_connection(-1)
    , m_address()
{
    printo("Creating TCP instance", NodeUtils::PrintType::CONNECTION);

    auto const networkComponents = options.GetBindingComponents();
    m_peerAddress = networkComponents.first;
    m_peerPort = networkComponents.second;

    memset(&m_address, 0, sizeof(m_address));

    Spawn();
    std::unique_lock<std::mutex> threadLock(m_mutex);
    this->m_cv.wait(threadLock, [this]{return m_active;});
    threadLock.unlock();
}

//------------------------------------------------------------------------------------------------

Connection::CTcp::~CTcp()
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
void Connection::CTcp::whatami()
{
    printo("[TCP] I am a TCP implementation", NodeUtils::PrintType::CONNECTION);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::Spawn()
{
    printo("[TCP] Spawning TCP_TYPE connection thread", NodeUtils::PrintType::CONNECTION);
    m_worker = std::thread(&CTcp::Worker, this);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns:
//------------------------------------------------------------------------------------------------
std::string const& Connection::CTcp::GetProtocolType() const
{
    static std::string const protocol = "IEEE 802.11";  // Not true, it is possible to use ethernet for example
    return protocol;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType Connection::CTcp::GetInternalType() const
{
    static NodeUtils::TechnologyType const internal = NodeUtils::TechnologyType::TCP;
    return internal;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::Worker()
{
    m_active = true;
    CreatePipe();

    switch (m_operation) {
        case NodeUtils::DeviceOperation::ROOT: {
            printo("[TCP] Setting up TCP socket on port " + m_port, NodeUtils::PrintType::CONNECTION);
            SetupTcpSocket(m_port);
        } break;
        case NodeUtils::DeviceOperation::BRANCH: break;
        case NodeUtils::DeviceOperation::LEAF: {
            printo("[TCP] Connecting TCP client socket to " + m_peerAddress + ":" + m_peerPort, NodeUtils::PrintType::CONNECTION);
            SetupTcpConnection(m_peerAddress, m_peerPort);
        } break;
        case NodeUtils::DeviceOperation::NONE: break;
    }

    // Notify the calling thread that the connection Worker is ready
    std::unique_lock<std::mutex> threadLock(m_mutex);
    m_cv.notify_one();
    threadLock.unlock();

    std::uint32_t run = 0;
    std::optional<std::string> optRequest;
    do {
        optRequest = Receive(0);
        if (optRequest) {
            WriteToPipe(*optRequest);
            optRequest.reset();
        }

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + local::timeout;
            auto const terminate = m_cv.wait_until(lock, stop, [&]{ return m_terminate; });
            // Terminate thread if the timeout was not reached
            if (terminate) {
                return;
            }
        }
        ++run;
    } while(true);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::SetupTcpSocket(NodeUtils::PortNumber const& port)
{
    // Creating socket file descriptor
    if (m_socket = socket(AF_INET, SOCK_STREAM, 0); m_socket < 0) {
        printo("[TCP] Socket failed", NodeUtils::PrintType::CONNECTION);
        return;
    }

    std::int32_t result = 0;
    result += setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &Connection::Tcp::OPT, sizeof(Connection::Tcp::OPT));
    result += setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &Connection::Tcp::OPT, sizeof(Connection::Tcp::OPT));
    if (result) {
        printo("[TCP] SetSockOpt failed", NodeUtils::PrintType::CONNECTION);
        return;
    }

    m_address.sin_family = AF_INET;
    m_address.sin_addr.s_addr = INADDR_ANY;
    std::int32_t portValue = std::stoi(port);
    m_address.sin_port = htons(portValue);

    if (bind(m_socket, reinterpret_cast<sockaddr *>(&m_address), sizeof(m_address)) < 0) {
        printo("[TCP] Bind failed", NodeUtils::PrintType::CONNECTION);
        return;
    }

    if (listen(m_socket, 30) < 0) {
        printo("[TCP] Listen failed", NodeUtils::PrintType::CONNECTION);
        return;
    }

    // Make the socket non-blocking
    if (fcntl(m_socket, F_SETFL, fcntl(m_socket, F_GETFL) | O_NONBLOCK) < 0) {
        return;
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::SetupTcpConnection(NodeUtils::IPv4Address const& address, NodeUtils::PortNumber const& port)
{
    if (m_connection = ::socket(AF_INET, SOCK_STREAM, 0); m_connection < 0) {
        return;
    }

    memset(&m_address, 0, sizeof(m_address));

    m_address.sin_family = AF_INET;
    std::int32_t portValue = std::stoi(port);
    m_address.sin_port = htons(portValue);

    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, address.c_str(), &m_address.sin_addr) <= 0) {
        return;
    }

    if (connect(m_connection, reinterpret_cast<sockaddr *>(&m_address), sizeof(m_address)) < 0) {
        return;
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::optional<std::string> Connection::CTcp::Receive(std::int32_t flag)
{
    if (m_connection < 0) {
        m_connection = accept(m_socket, (struct sockaddr *)&m_address, (socklen_t*)&Connection::Tcp::ADDRESS_SIZE);
        if (fcntl(m_socket, F_SETFL, fcntl(m_socket, F_GETFL) | flag) < 0) {
            return {};
        }
    }

    if (fcntl(m_connection, F_SETFL, fcntl(m_connection, F_GETFL) | flag) < 0) {
        return {};
    }

    char buffer[Connection::Tcp::BUFFER_SIZE];
    memset(buffer, 0, Connection::Tcp::BUFFER_SIZE);
    std::int32_t bytesRead = read(m_connection, buffer, Connection::Tcp::BUFFER_SIZE);
    printo("[TCP] Received: " + std::to_string(bytesRead) + " " + std::string(buffer), NodeUtils::PrintType::CONNECTION);

    return std::string(buffer, strlen(buffer));
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::string Connection::CTcp::InternalReceive()
{
    char buffer[Connection::Tcp::BUFFER_SIZE];
    memset(buffer, 0, Connection::Tcp::BUFFER_SIZE);
    std::int32_t bytesRead = read(m_connection, buffer, Connection::Tcp::BUFFER_SIZE);
    printo("[TCP] Received: (" + std::to_string(bytesRead) + ") " + std::string(buffer), NodeUtils::PrintType::CONNECTION);

    return std::string(buffer, strlen(buffer));
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::Send(CMessage const& message)
{
    std::string pack = message.GetPack();
    std::int32_t bytesSent = ::send(m_connection, pack.c_str(), strlen(pack.c_str()), 0);
    printo("[TCP] Sent: (" + std::to_string(bytesSent) + ") " + pack, NodeUtils::PrintType::CONNECTION);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::Send(char const* const message)
{
    std::int32_t bytesSent = ::send(m_connection, message, strlen(message), 0);
    printo("[TCP] Sent: (" + std::to_string(bytesSent) + ") " + std::string(message),NodeUtils::PrintType::CONNECTION);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::PrepareForNext()
{
    close(m_connection);
    m_connection = -1;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
bool Connection::CTcp::Shutdown()
{
    printo("[TCP] Shutting down socket and context", NodeUtils::PrintType::CONNECTION);
    // Stop the worker thread from processing the connections
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        close(m_connection);
        close(m_socket);
        m_terminate = true;
    }

    m_cv.notify_all();
    
    if (m_worker.joinable()) {
        m_worker.join();
    }
    
    return !m_worker.joinable();
}

//------------------------------------------------------------------------------------------------
