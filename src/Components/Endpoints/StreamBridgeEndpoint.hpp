//------------------------------------------------------------------------------------------------
// File: StreamBridgeEndpoint.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "ConnectionDetails.hpp"
#include "ConnectionTracker.hpp"
#include "TechnologyType.hpp"
//------------------------------------------------------------------------------------------------
#include <any>
#include <deque>
#include <variant>
#include <zmq.hpp>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace StreamBridge {
//------------------------------------------------------------------------------------------------

struct TNetworkInstructionEvent;
struct TOutgoingMessageEvent;

//------------------------------------------------------------------------------------------------
} // StreamBridge namespace
//------------------------------------------------------------------------------------------------

struct StreamBridge::TNetworkInstructionEvent {
    TNetworkInstructionEvent(
        CEndpoint::NetworkInstruction type,
        std::string_view address,
        NetworkUtils::PortNumber port)
        : type(type)
        , address(address)
        , port(port)
    {
    };
    CEndpoint::NetworkInstruction const type;
    NetworkUtils::NetworkAddress const address;
    NetworkUtils::PortNumber const port;
};

//------------------------------------------------------------------------------------------------

struct StreamBridge::TOutgoingMessageEvent {
    TOutgoingMessageEvent(
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

    constexpr static std::string_view Scheme = "tcp://";
    constexpr static std::string_view ProtocolType = "TCP/IP";
    constexpr static TechnologyType InternalType = TechnologyType::StreamBridge;

    CStreamBridgeEndpoint(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        std::string_view interface,
        OperationType operation,
        IEndpointMediator const* const pEndpointMediator,
        IPeerMediator* const pPeerMediator,
        IMessageSink* const pMessageSink);
    ~CStreamBridgeEndpoint() override;

    // CEndpoint{
    TechnologyType GetInternalType() const override;
    std::string GetProtocolType() const override;
    std::string GetEntry() const override;
    std::string GetURI() const override;

    void ScheduleBind(std::string_view binding) override;
    void ScheduleConnect(std::string_view entry) override;
    void Startup() override;

    bool ScheduleSend(CMessage const& message) override;
    bool ScheduleSend(
        BryptIdentifier::CContainer const& identifier,
        std::string_view message) override;

    bool Shutdown() override;
    // }CEndpoint

private:
    enum class ConnectionStateChange : std::uint8_t { Update };

    using NetworkInstructionDeque = std::deque<StreamBridge::TNetworkInstructionEvent>;
    using OutgoingMessageDeque = std::deque<StreamBridge::TOutgoingMessageEvent>;

    using ReceiveResult = std::variant<ConnectionStateChange, std::string>;
    using OptionalReceiveResult = std::optional<std::pair<ZeroMQIdentity, ReceiveResult>>;

    using ExtendedConnectionDetails = CConnectionDetails<void>;

    void Spawn();
    bool SetupServerWorker();
    void ServerWorker();
    bool Listen(
        zmq::socket_t& socket,
        NetworkUtils::NetworkAddress const& address,
        NetworkUtils::PortNumber port);

    void ProcessNetworkInstructions(zmq::socket_t& socket);

    void ProcessIncomingMessages(zmq::socket_t& socket);
    OptionalReceiveResult Receive(zmq::socket_t& socket);
    void HandleReceivedData(ZeroMQIdentity const& identity, std::string_view message);

    void ProcessOutgoingMessages(zmq::socket_t& socket);
    std::uint32_t Send(
        zmq::socket_t& socket,
        ZeroMQIdentity const& identity,
        std::string_view message);

    void HandleConnectionStateChange(ZeroMQIdentity const& identity, ConnectionStateChange change);

    NetworkUtils::NetworkAddress m_address;
    NetworkUtils::PortNumber m_port;

    CConnectionTracker<ZeroMQIdentity> m_tracker;

    mutable std::mutex m_eventsMutex;
    EventDeque m_events;
};

//------------------------------------------------------------------------------------------------
