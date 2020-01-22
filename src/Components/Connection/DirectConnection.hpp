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
namespace Direct {
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
} // Direct namespace
} // Connection namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class Connection::CDirect : public CConnection {
public:
    CDirect(
        IMessageSink* const messageSink,
        Configuration::TConnectionOptions const& options);
    ~CDirect() override;

    // CConnection{
    void whatami() override;
    std::string const& GetProtocolType() const override;
    NodeUtils::TechnologyType GetInternalType() const override;

    void Spawn() override;
    void Worker() override;

    void HandleProcessedMessage(std::string_view message) override;
    void Send(CMessage const& message) override;
    void Send(std::string_view message) override;
    std::optional<std::string> Receive(std::int32_t flag = 0) override;

    void PrepareForNext() override;
    bool Shutdown() override;
    // }CConnection

    void SetupServerWorker(
        NodeUtils::PortNumber const& port);
    void SetupClientWorker(
        NodeUtils::NetworkAddress const& address,
        NodeUtils::PortNumber const& port);

    void ServerWorker();
    void ClientWorker();

    std::optional<std::string> Receive(zmq::recv_flags flag);

private:
	NodeUtils::PortNumber m_port;
	NodeUtils::NetworkAddress m_peerAddress;
	NodeUtils::PortNumber m_peerPort;

	std::unique_ptr<zmq::context_t> m_context;
	std::unique_ptr<zmq::socket_t> m_socket;
	zmq::pollitem_t m_pollItem;
    
    std::atomic_bool m_bProcessReceived;
};

//------------------------------------------------------------------------------------------------
