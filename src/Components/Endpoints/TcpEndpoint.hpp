//------------------------------------------------------------------------------------------------
// File: TcpEndpoint.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "PeerDetails.hpp"
#include "PeerDetailsMap.hpp"
#include "TechnologyType.hpp"
#include "../../Message/MessageTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <any>
#include <deque>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <variant>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Tcp {
//------------------------------------------------------------------------------------------------

struct TNetworkInstructionEvent;
struct TOutgoingMessageEvent;

constexpr std::int32_t InvalidDescriptor = -1;

//------------------------------------------------------------------------------------------------
} // Tcp namespace
//------------------------------------------------------------------------------------------------

struct Tcp::TNetworkInstructionEvent {
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

struct Tcp::TOutgoingMessageEvent {
    TOutgoingMessageEvent(
        std::int32_t descriptor,
        std::string_view message,
        std::uint8_t retries)
        : descriptor(descriptor)
        , message(message)
        , retries(retries)
    {
    };
    std::int32_t descriptor;
    std::string message;
    std::uint8_t retries;
};

//------------------------------------------------------------------------------------------------

class Endpoints::CTcpEndpoint : public CEndpoint {
public:
    using SocketDescriptor = std::int32_t;
    using IPv4SocketAddress = struct sockaddr_in;
    
    constexpr static std::string_view Scheme = "tcp://";
    constexpr static std::string_view ProtocolType = "TCP/IP";
    constexpr static TechnologyType InternalType = TechnologyType::TCP;

    CTcpEndpoint(
        NodeUtils::NodeIdType id,
        std::string_view interface,
        OperationType operation,
        IEndpointMediator const* const pEndpointMediator,
        IPeerMediator* const pPeerMediator,
        IMessageSink* const pMessageSink);
    ~CTcpEndpoint() override;

    // CEndpoint{
    TechnologyType GetInternalType() const override;
    std::string GetProtocolType() const override;
    std::string GetEntry() const override;
    std::string GetURI() const override;

    void ScheduleBind(std::string_view binding) override;
    void ScheduleConnect(std::string_view entry) override;
    void Startup() override;

    bool ScheduleSend(CMessage const& message) override;
    bool ScheduleSend(NodeUtils::NodeIdType id, std::string_view message) override;

    bool Shutdown() override;
    // }CEndpoint
    
private:
    enum class ConnectionStateChange : std::uint8_t { Connect, Disconnect };
    enum class ConnectStatusCode : std::uint8_t {
        Success,
        Terminated,
        GenericError,
        RetryError,
        ReflectionError,
        DuplicateError
    };

    using NetworkInstructionDeque = std::deque<Tcp::TNetworkInstructionEvent>;
    using OutgoingMessageDeque = std::deque<Tcp::TOutgoingMessageEvent>;

    using ReceiveResult = std::variant<ConnectionStateChange, Message::Buffer>;
    using OptionalReceiveResult = std::optional<ReceiveResult>;
    using SendResult = std::variant<ConnectionStateChange, std::int32_t>;

    using ExtendedPeerDetails = CPeerDetails<void>;

    constexpr static std::int32_t SocketAddressSize = sizeof(IPv4SocketAddress);
    constexpr static std::int32_t ReadBufferSize = 8192;

    void Spawn();

    bool SetupServerWorker();
    void ServerWorker();
    bool Listen(
        SocketDescriptor* descriptor,
        NetworkUtils::NetworkAddress const& address,
        NetworkUtils::PortNumber port,
        IPv4SocketAddress& socketAddress);
    void AcceptConnection(SocketDescriptor listenDescriptor);

    bool SetupClientWorker();
    void ClientWorker();
    ConnectStatusCode Connect(
        NetworkUtils::NetworkAddress const& address,
        NetworkUtils::PortNumber port,
        IPv4SocketAddress& socketAddress);
    ConnectStatusCode IsURIAllowed(std::string_view uri);
    ConnectStatusCode EstablishConnection(
        SocketDescriptor descriptor, IPv4SocketAddress address);

    void ProcessNetworkInstructions(SocketDescriptor* listener = nullptr);

    void ProcessIncomingMessages();
    OptionalReceiveResult Receive(SocketDescriptor descriptor);
    void HandleReceivedData(SocketDescriptor descriptor, Message::Buffer const& buffer);

    void ProcessOutgoingMessages();
    SendResult Send(SocketDescriptor descriptor, std::string_view message);
    
    void HandleConnectionStateChange(SocketDescriptor descriptor, ConnectionStateChange change);

    NetworkUtils::NetworkAddress m_address;
    NetworkUtils::PortNumber m_port;

    CPeerDetailsMap<SocketDescriptor> m_peers;

    mutable std::mutex m_eventsMutex;
    EventDeque m_events;
};

//------------------------------------------------------------------------------------------------
