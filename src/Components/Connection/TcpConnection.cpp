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

Connection::CTcp::CTcp(
    IMessageSink* const messageSink,
    Configuration::TConnectionOptions const& options)
    : CConnection(messageSink, options)
    , m_port()
    , m_peerAddress()
    , m_peerPort()
    , m_socket(0)
    , m_connection(-1)
    , m_address()
{
    NodeUtils::printo("Creating TCP instance", NodeUtils::PrintType::Connection);

    auto const bindingComponents = options.GetBindingComponents();
    auto const peerComponents = options.GetBindingComponents();

    m_port = bindingComponents.second;
    m_peerAddress = peerComponents.first;
    m_peerPort = peerComponents.second;

    memset(&m_address, 0, sizeof(m_address));

    Spawn();
    std::unique_lock lock(m_mutex);
    this->m_cv.wait(lock, [this]{return m_active.load();});
    lock.unlock();
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
    NodeUtils::printo("[TCP] I am a TCP implementation", NodeUtils::PrintType::Connection);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::Spawn()
{
    NodeUtils::printo("[TCP] Spawning TCP connection thread", NodeUtils::PrintType::Connection);
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

    switch (m_operation) {
        case NodeUtils::ConnectionOperation::Server: {
            NodeUtils::printo("[TCP] Setting up TCP socket on port " + m_port, NodeUtils::PrintType::Connection);
            SetupTcpSocket(m_port);
        } break;
        case NodeUtils::ConnectionOperation::Client: {
            NodeUtils::printo("[TCP] Connecting TCP client socket to " + m_peerAddress + ":" + m_peerPort, NodeUtils::PrintType::Connection);
            SetupTcpConnection(m_peerAddress, m_peerPort);
        } break;
        default: break;
    }

    // Notify the calling thread that the connection Worker is ready
    m_cv.notify_one();

    std::uint32_t run = 0;
    std::optional<std::string> optReceivedRaw;
    do {
        optReceivedRaw = Receive(0);
        if (optReceivedRaw) {
            std::scoped_lock lock(m_mutex);

            try {
                CMessage const request(*optReceivedRaw);
                m_messageSink->ForwardMessage(m_id, request);
            } catch (...) {
                NodeUtils::printo("[TCP] Received message failed to unpack.", NodeUtils::PrintType::Connection);
            }

            optReceivedRaw.reset();
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
void Connection::CTcp::SetupTcpSocket(NodeUtils::PortNumber const& port)
{
    // Creating socket file descriptor
    if (m_socket = socket(AF_INET, SOCK_STREAM, 0); m_socket < 0) {
        NodeUtils::printo("[TCP] Socket failed", NodeUtils::PrintType::Connection);
        return;
    }

    std::int32_t result = 0;
    result += setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &Connection::Tcp::OPT, sizeof(Connection::Tcp::OPT));
    result += setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &Connection::Tcp::OPT, sizeof(Connection::Tcp::OPT));
    if (result) {
        NodeUtils::printo("[TCP] SetSockOpt failed", NodeUtils::PrintType::Connection);
        return;
    }

    m_address.sin_family = AF_INET;
    m_address.sin_addr.s_addr = INADDR_ANY;
    std::int32_t portValue = std::stoi(port);
    m_address.sin_port = htons(portValue);

    if (bind(m_socket, reinterpret_cast<sockaddr *>(&m_address), sizeof(m_address)) < 0) {
        NodeUtils::printo("[TCP] Bind failed", NodeUtils::PrintType::Connection);
        return;
    }

    if (listen(m_socket, 30) < 0) {
        NodeUtils::printo("[TCP] Listen failed", NodeUtils::PrintType::Connection);
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
void Connection::CTcp::SetupTcpConnection(
    NodeUtils::NetworkAddress const& address,
    NodeUtils::PortNumber const& port)
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
void Connection::CTcp::HandleProcessedMessage(CMessage const& /*message*/)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::Send(CMessage const& message)
{
    std::string pack = message.GetPack();
    std::int32_t bytesSent = ::send(m_connection, pack.c_str(), strlen(pack.c_str()), 0);
    NodeUtils::printo("[TCP] Sent: (" + std::to_string(bytesSent) + ") " + pack, NodeUtils::PrintType::Connection);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Connection::CTcp::Send(std::string_view message)
{
    std::int32_t bytesSent = ::send(m_connection, message.data(), message.size(), 0);
    NodeUtils::printo("[TCP] Sent: (" + std::to_string(bytesSent) + ") " + message.data(),NodeUtils::PrintType::Connection);
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
    NodeUtils::printo("[TCP] Received: " + std::to_string(bytesRead) + " " + std::string(buffer), NodeUtils::PrintType::Connection);

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
    NodeUtils::printo("[TCP] Received: (" + std::to_string(bytesRead) + ") " + std::string(buffer), NodeUtils::PrintType::Connection);

    return std::string(buffer, strlen(buffer));
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
    NodeUtils::printo("[TCP] Shutting down socket and context", NodeUtils::PrintType::Connection);
    // Stop the worker thread from processing the connections
    {
        std::scoped_lock lock(m_mutex);
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
