//----------------------------------------------------------------------------------------------------------------------
// File: BryptPeer.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "EndpointRegistration.hpp"
#include "PeerStatistics.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/MessageScheduler.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/MessageSink.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

class MessageContext;
class SecurityMediator;
namespace Network { class Address; }

class IPeerMediator;

//----------------------------------------------------------------------------------------------------------------------

class BryptPeer : public std::enable_shared_from_this<BryptPeer>
{
public:
    explicit BryptPeer(
        BryptIdentifier::Container const& identifier,
        IPeerMediator* const pPeerMediator = nullptr);

    ~BryptPeer();

    [[nodiscard]] BryptIdentifier::SharedContainer GetBryptIdentifier() const;
    [[nodiscard]] BryptIdentifier::Internal::Type GetInternalIdentifier() const;

    // Statistic Methods {
    [[nodiscard]] std::uint32_t GetSentCount() const;
    [[nodiscard]] std::uint32_t GetReceivedCount() const;
    // } Statistic Methods

    // Message Receiving Methods {
    void SetReceiver(IMessageSink* const pMessageSink);
    [[nodiscard]] bool ScheduleReceive(
        Network::Endpoint::Identifier identifier, std::string_view buffer);
    [[nodiscard]] bool ScheduleReceive(
        Network::Endpoint::Identifier identifier, std::span<std::uint8_t const> buffer);
    // } Message Receiving Methods

    // Message Sending Methods {
    [[nodiscard]] bool ScheduleSend(
        Network::Endpoint::Identifier identifier, std::string_view message) const;
    // } Message Sending Methods

    // Endpoint Association Methods {
    void RegisterEndpoint(EndpointRegistration const& registration);
    void RegisterEndpoint(
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        Network::RemoteAddress const& address = {},
        MessageScheduler const& scheduler = {});

    void WithdrawEndpoint(
        Network::Endpoint::Identifier identifier, Network::Protocol protocol);

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool IsEndpointRegistered(Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::optional<MessageContext> GetMessageContext(
        Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::optional<Network::RemoteAddress> GetRegisteredAddress(
        Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::size_t RegisteredEndpointCount() const;
    // } Endpoint Association Methods

    // Security Methods {
    void AttachSecurityMediator(std::unique_ptr<SecurityMediator>&& upSecurityMediator);
    [[nodiscard]] Security::State GetSecurityState() const;
    [[nodiscard]] bool IsFlagged() const;
    [[nodiscard]] bool IsAuthorized() const;
    // } Security Methods
    
private:
    using RegisteredEndpoints = std::unordered_map<
        Network::Endpoint::Identifier, EndpointRegistration>;

    IPeerMediator* const m_pPeerMediator;

    mutable std::recursive_mutex m_dataMutex;
    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    mutable PeerStatistics m_statistics;

    mutable std::recursive_mutex m_mediatorMutex;
    std::unique_ptr<SecurityMediator> m_upSecurityMediator;

    mutable std::recursive_mutex m_endpointsMutex;
    RegisteredEndpoints m_endpoints;

    mutable std::recursive_mutex m_receiverMutex;
    IMessageSink* m_pMessageSink;
};

//----------------------------------------------------------------------------------------------------------------------
