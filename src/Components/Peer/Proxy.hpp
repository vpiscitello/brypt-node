//----------------------------------------------------------------------------------------------------------------------
// File: Proxy.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Registration.hpp"
#include "Statistics.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "BryptMessage/ShareablePack.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/MessageScheduler.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Utilities/InvokeContext.hpp"
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
namespace Security{ class Mediator; }
namespace Network { class Address; }

class IPeerMediator;

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Proxy;

//----------------------------------------------------------------------------------------------------------------------
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Proxy final : public std::enable_shared_from_this<Peer::Proxy>
{
public:
    explicit Proxy(Node::Identifier const& identifier, IPeerMediator* const pPeerMediator = nullptr);
    ~Proxy();

    [[nodiscard]] Node::SharedIdentifier GetNodeIdentifier() const;
    [[nodiscard]] Node::Internal::Identifier::Type GetInternalIdentifier() const;

    // Statistic Methods {
    [[nodiscard]] std::uint32_t GetSentCount() const;
    [[nodiscard]] std::uint32_t GetReceivedCount() const;
    // } Statistic Methods

    // Message Receipt Methods {
    void SetReceiver(IMessageSink* const pMessageSink);
    [[nodiscard]] bool ScheduleReceive(
        Network::Endpoint::Identifier identifier, std::string_view buffer);
    [[nodiscard]] bool ScheduleReceive(
        Network::Endpoint::Identifier identifier, std::span<std::uint8_t const> buffer);
    // } Message Receipt Methods

    // Message Dispatch Methods {
    [[nodiscard]] bool ScheduleSend(Network::Endpoint::Identifier identifier, std::string&& message) const;
    [[nodiscard]] bool ScheduleSend(
        Network::Endpoint::Identifier identifier, Message::ShareablePack const& spSharedPack) const;
    // } Message Dispatch Methods

    // Endpoint Association Methods {
    void RegisterEndpoint(Registration const& registration);
    void RegisterEndpoint(
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        Network::RemoteAddress const& address = {},
        Network::MessageScheduler const& scheduler = {});

    void WithdrawEndpoint(Network::Endpoint::Identifier identifier, Network::Protocol protocol);

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool IsEndpointRegistered(Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::optional<MessageContext> GetMessageContext(
        Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::optional<Network::RemoteAddress> GetRegisteredAddress(
        Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::size_t RegisteredEndpointCount() const;
    // } Endpoint Association Methods

    // Security Methods {
    void AttachSecurityMediator(std::unique_ptr<Security::Mediator>&& upSecurityMediator);
    [[nodiscard]] Security::State GetSecurityState() const;
    [[nodiscard]] bool IsFlagged() const;
    [[nodiscard]] bool IsAuthorized() const;
    // } Security Methods

    // Testing Methods {
    template <InvokeContext ContextType = InvokeContext::Production> requires TestingContext<ContextType>
    void RegisterSilentEndpoint(Registration const& registration);

    template <InvokeContext ContextType = InvokeContext::Production> requires TestingContext<ContextType>
    void RegisterSilentEndpoint(
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        Network::RemoteAddress const& address = {},
        Network::MessageScheduler const& scheduler = {});

    template <InvokeContext ContextType = InvokeContext::Production> requires TestingContext<ContextType>
    void WithdrawSilentEndpoint(Network::Endpoint::Identifier identifier, Network::Protocol protocol);
    // } Testing Methods
    
private:
    using RegisteredEndpoints = std::unordered_map<Network::Endpoint::Identifier, Registration>;

    Node::SharedIdentifier m_spIdentifier;
    mutable Statistics m_statistics;

    IPeerMediator* const m_pPeerMediator;
    
    mutable std::recursive_mutex m_securityMutex;
    std::unique_ptr<Security::Mediator> m_upSecurityMediator;

    mutable std::recursive_mutex m_endpointsMutex;
    RegisteredEndpoints m_endpoints;

    mutable std::recursive_mutex m_receiverMutex;
    IMessageSink* m_pMessageSink;
};

//----------------------------------------------------------------------------------------------------------------------
