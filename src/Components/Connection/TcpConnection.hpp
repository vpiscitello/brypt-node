//------------------------------------------------------------------------------------------------
// File: TcpConnection.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Connection.hpp"
//------------------------------------------------------------------------------------------------
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Connection {
//------------------------------------------------------------------------------------------------
namespace Tcp {
//------------------------------------------------------------------------------------------------
constexpr std::int32_t OPT = 1;
constexpr std::int32_t ADDRESS_SIZE = sizeof(struct sockaddr_in);
constexpr std::int32_t BUFFER_SIZE = 1024;
//------------------------------------------------------------------------------------------------
} // Tcp namespace
//------------------------------------------------------------------------------------------------
} // Connection namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class Connection::CTcp : public CConnection {
public:
    CTcp();
    explicit CTcp(NodeUtils::TOptions const& options);
    ~CTcp() override {};

    void whatami() override;
    std::string const& GetProtocolType() const override;
    NodeUtils::TechnologyType const& GetInternalType() const override;

    void Spawn() override;
    void Worker() override;
    void SetupTcpSocket(NodeUtils::PortNumber const& port);
    void SetupTcpConnection(NodeUtils::IPv4Address const& address, NodeUtils::PortNumber const& port);

    void Send(CMessage const& message) override;
    void Send(char const* const message) override;
    std::optional<std::string> Receive(std::int32_t flag = 0) override;
    std::string InternalReceive();

    void PrepareForNext() override;
    void Shutdown() override;

private:
    bool m_control;

    NodeUtils::PortNumber m_port;
    std::string m_peerAddress;
    std::string m_peerPort;

    std::int32_t m_socket;
    std::int32_t m_connection;
    struct sockaddr_in m_address;
};

//------------------------------------------------------------------------------------------------
