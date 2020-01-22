//------------------------------------------------------------------------------------------------
// File: StreamBridgeConnection.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Connection.hpp"
//------------------------------------------------------------------------------------------------
#include <zmq.hpp>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Connection {
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
namespace StreamBridge {
//------------------------------------------------------------------------------------------------
constexpr std::uint32_t ID_SIZE = 256;
constexpr std::uint32_t BUFFER_SIZE = 512;
//------------------------------------------------------------------------------------------------
} // StreamBridge namespace
//------------------------------------------------------------------------------------------------
} // Connection namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

// ZMQ StreamBridge socket implementation
class Connection::CStreamBridge : public CConnection {
public:
    CStreamBridge(
        IMessageSink* const messageSink,
        Configuration::TConnectionOptions const& options);
    ~CStreamBridge() override;

    void whatami() override;
    std::string const& GetProtocolType() const override;
    NodeUtils::TechnologyType GetInternalType() const override;

    void Spawn() override;
    void Worker() override;
    void SetupStreamBridgeSocket(NodeUtils::PortNumber const& port);

    void HandleProcessedMessage(std::string_view message) override;
    void Send(CMessage const& message) override;
    void Send(std::string_view message) override;
    std::optional<std::string> Receive(std::int32_t flag = 0) override;

    void PrepareForNext() override;
    bool Shutdown() override;

private:
    std::uint8_t m_streamId[StreamBridge::ID_SIZE];

    NodeUtils::PortNumber m_port;
    NodeUtils::NetworkAddress m_peerAddress;
    NodeUtils::PortNumber m_peerPort;

    std::int32_t m_initializationMessage;

    std::unique_ptr<zmq::context_t> m_context;
	std::unique_ptr<zmq::socket_t> m_socket;
    
};

//------------------------------------------------------------------------------------------------
