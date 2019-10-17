//------------------------------------------------------------------------------------------------
// File: DirectConnection.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Connection.hpp"
//------------------------------------------------------------------------------------------------
#include <zmq.hpp>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Connection {
//------------------------------------------------------------------------------------------------
namespace Direct {
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
} // Direct namespace
//------------------------------------------------------------------------------------------------
} // Connection namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class Connection::CDirect : public CConnection {
public:
    CDirect();
    explicit CDirect(NodeUtils::TOptions const& options);
    ~CDirect() override {};

    void whatami();
    std::string const& GetProtocolType() const;
    NodeUtils::TechnologyType const& GetInternalType() const;

    void Spawn();
    void Worker();
    void SetupResponseSocket(NodeUtils::PortNumber const& port);
    void SetupRequestSocket(NodeUtils::IPv4Address const& address, NodeUtils::PortNumber const& port);

    void Send(CMessage const& message);
    void Send(char const* const message);
    std::optional<std::string> Receive(std::int32_t flag = 0);
    void PrepareForNext();
    void Shutdown();
    void HandleMessaging();

private:
	bool m_control;

	NodeUtils::PortNumber m_port;
	NodeUtils::IPv4Address m_peerAddress;
	NodeUtils::PortNumber m_peerPort;

	std::unique_ptr<zmq::context_t> m_context;
	std::unique_ptr<zmq::socket_t> m_socket;
	zmq::pollitem_t m_pollItem;
};

//------------------------------------------------------------------------------------------------
