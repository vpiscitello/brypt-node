//------------------------------------------------------------------------------------------------
// File: TcpEndpoint.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "PeerDetails.hpp"
#include "PeerDetailsMap.hpp"
#include "../../Utilities/MessageTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <deque>
#include <fcntl.h>
#include <unistd.h>
#include <variant>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Tcp {
//------------------------------------------------------------------------------------------------

struct TOutgoingMessageInformation;

constexpr std::int32_t InvalidDescriptor = -1;

//------------------------------------------------------------------------------------------------
} // Tcp namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

struct Tcp::TOutgoingMessageInformation {
    TOutgoingMessageInformation(
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
    
    constexpr static std::string_view ProtocolType = "TCP/IP";
    constexpr static NodeUtils::TechnologyType InternalType = NodeUtils::TechnologyType::TCP;

    CTcpEndpoint(
        IMessageSink* const messageSink,
        Configuration::TEndpointOptions const& options);
    ~CTcpEndpoint() override;

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
    enum class ConnectionStateChange : std::uint8_t { Connect, Disconnect };

    using OutgoingMessageDeque = std::deque<Tcp::TOutgoingMessageInformation>;

    using ReceiveResult = std::variant<ConnectionStateChange, Message::Buffer>;
    using OptionalReceiveResult = std::optional<ReceiveResult>;
    using SendResult = std::variant<ConnectionStateChange, std::int32_t>;

    constexpr static std::int32_t SocketAddressSize = sizeof(IPv4SocketAddress);
    constexpr static std::int32_t ReadBufferSize = 8192;

    void Spawn();

    bool SetupServerWorker();
    void ServerWorker();
    SocketDescriptor Listen(
        NodeUtils::NetworkAddress const& address,
        NodeUtils::PortNumber const& port,
        IPv4SocketAddress& socketAddress);
    void AcceptConnection(SocketDescriptor listenDescriptor);

    bool SetupClientWorker();
    void ClientWorker();
    SocketDescriptor Connect(
        NodeUtils::NetworkAddress const& address,
        NodeUtils::PortNumber const& port,
        IPv4SocketAddress& socketAddress);
    bool EstablishConnection(
        SocketDescriptor descriptor, IPv4SocketAddress address);
    void StartPeerAuthentication(SocketDescriptor descriptor);

    void ProcessIncomingMessages();
    OptionalReceiveResult Receive(SocketDescriptor descriptor);
    void HandleReceivedData(SocketDescriptor descriptor, Message::Buffer const& buffer);

    void ProcessOutgoingMessages();
    SendResult Send(SocketDescriptor descriptor, std::string_view message);
    
    void HandleConnectionStateChange(SocketDescriptor descriptor, ConnectionStateChange change);

    NodeUtils::NetworkAddress m_address;
    NodeUtils::PortNumber m_port;

    std::string m_peerAddress;
    std::string m_peerPort;

    CPeerInformationMap<SocketDescriptor> m_peers;

    mutable std::mutex m_outgoingMutex;
    OutgoingMessageDeque m_outgoing;
};

//------------------------------------------------------------------------------------------------
