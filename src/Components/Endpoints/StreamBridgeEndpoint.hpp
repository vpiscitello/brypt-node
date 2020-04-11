//------------------------------------------------------------------------------------------------
// File: StreamBridgeEndpoint.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "PeerDetails.hpp"
#include "PeerDetailsMap.hpp"
//------------------------------------------------------------------------------------------------
#include <deque>
#include <variant>
#include <zmq.hpp>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace StreamBridge {
//------------------------------------------------------------------------------------------------

struct TOutgoingMessageInformation;

//------------------------------------------------------------------------------------------------
} // StreamBridge namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

struct StreamBridge::TOutgoingMessageInformation {
    TOutgoingMessageInformation(
        std::string_view identity,
        std::string_view message,
        std::uint8_t retries)
        : identity(identity)
        , message(message)
        , retries(retries)
    {
    };
    std::string identity;
    std::string message;
    std::uint8_t retries;
};

//------------------------------------------------------------------------------------------------

class Endpoints::CStreamBridgeEndpoint : public CEndpoint {
public:
    using ZeroMQIdentity = std::string;
    
    constexpr static std::string_view ProtocolType = "TCP/IP";
    constexpr static NodeUtils::TechnologyType InternalType = NodeUtils::TechnologyType::StreamBridge;

    CStreamBridgeEndpoint(
        IMessageSink* const messageSink,
        Configuration::TEndpointOptions const& options);
    ~CStreamBridgeEndpoint() override;

    // CEndpoint{
    std::string GetProtocolType() const override;
    NodeUtils::TechnologyType GetInternalType() const override;

    void Startup() override;

    void HandleProcessedMessage(NodeUtils::NodeIdType id, CMessage const& message) override;
    void Send(NodeUtils::NodeIdType id, CMessage const& message) override;
    void Send(NodeUtils::NodeIdType id, std::string_view message) override;

    bool Shutdown() override;
    // }CEndpoint

private:
    enum class ConnectionStateChange : std::uint8_t { Update };
    
    using OutgoingMessageDeque = std::deque<StreamBridge::TOutgoingMessageInformation>;
    using ReceiveResult = std::variant<ConnectionStateChange, std::string>;
    using OptionalReceiveResult = std::optional<std::pair<ZeroMQIdentity, ReceiveResult>>;

    void Spawn();
    bool SetupServerWorker();
    void ServerWorker();
    void Listen(
        zmq::socket_t& socket,
        NodeUtils::NetworkAddress const& address,
        NodeUtils::PortNumber const& port);

    void ProcessIncomingMessages(zmq::socket_t& socket);
    OptionalReceiveResult Receive(zmq::socket_t& socket);
    void HandleReceivedData(ZeroMQIdentity const& identity, std::string_view message);

    void ProcessOutgoingMessages(zmq::socket_t& socket);
    std::uint32_t Send(
        zmq::socket_t& socket,
        ZeroMQIdentity id,
        std::string_view message);

    void HandleConnectionStateChange(ZeroMQIdentity const& identity, ConnectionStateChange change);

    NodeUtils::NetworkAddress m_address;
    NodeUtils::PortNumber m_port;

    CPeerInformationMap<ZeroMQIdentity> m_peers;

    mutable std::mutex m_outgoingMutex;
    OutgoingMessageDeque m_outgoing;
};

//------------------------------------------------------------------------------------------------
