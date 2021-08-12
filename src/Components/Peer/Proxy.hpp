//----------------------------------------------------------------------------------------------------------------------
// File: Proxy.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Registration.hpp"
#include "Resolver.hpp"
#include "Statistics.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "BryptMessage/ShareablePack.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/MessageScheduler.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/SecurityStrategy.hpp"
#include "Utilities/InvokeContext.hpp"
#include "Utilities/TokenizedInstance.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

class MessageContext;
class IConnectProtocol;
class IPeerMediator;

namespace Network { class Address; }

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Proxy;

//----------------------------------------------------------------------------------------------------------------------
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Proxy final : public std::enable_shared_from_this<Proxy>, public TokenizedInstance<Proxy>
{
public:
    // Note: An instance of the proxy must be created through the inherited CreateInstance() method. This is to ensure
    // all instances are held within an std::shared_ptr, otherwise shared_from_this() can lead to undefined behavior. 
    explicit Proxy(
        InstanceToken,
        Node::Identifier const& identifier,
        std::weak_ptr<IMessageSink> const& wpProcessor = {},
        IPeerMediator* const pMediator = nullptr);

    ~Proxy();

    [[nodiscard]] Node::SharedIdentifier GetIdentifier() const;
    [[nodiscard]] Node::Internal::Identifier GetInternalIdentifier() const;

    // Statistic Methods {
    [[nodiscard]] std::uint32_t GetSentCount() const;
    [[nodiscard]] std::uint32_t GetReceivedCount() const;
    // } Statistic Methods

    // Message Receipt Methods {
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
    void WithdrawEndpoint(Network::Endpoint::Identifier identifier);

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool IsEndpointRegistered(Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::optional<MessageContext> GetMessageContext(
        Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::optional<Network::RemoteAddress> GetRegisteredAddress(
        Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::size_t RegisteredEndpointCount() const;
    // } Endpoint Association Methods

    // Security Methods {
    [[nodiscard]] bool AttachResolver(std::unique_ptr<Resolver>&& upResolver);
    [[nodiscard]] bool StartExchange(
        Node::SharedIdentifier const& spSource,
        Security::Strategy strategy,
        Security::Role role,
        std::shared_ptr<IConnectProtocol> const& spProtocol);
    [[nodiscard]] Security::State GetAuthorization() const;
    [[nodiscard]] bool IsFlagged() const;
    [[nodiscard]] bool IsAuthorized() const;
    // } Security Methods

    // Testing Methods {
    UT_SupportMethod(void SetReceiver(IMessageSink* const pMessageSink));
    UT_SupportMethod(void SetAuthorization(Security::State state));
    UT_SupportMethod(void AttachSecurityStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy));
    UT_SupportMethod(void DetachResolver());
    UT_SupportMethod(void RegisterSilentEndpoint(Registration const& registration));
    UT_SupportMethod(
        void RegisterSilentEndpoint(
            Network::Endpoint::Identifier identifier,
            Network::Protocol protocol,
            Network::RemoteAddress const& address = {},
            Network::MessageScheduler const& scheduler = {}));
    UT_SupportMethod(void WithdrawSilentEndpoint(Network::Endpoint::Identifier identifier, Network::Protocol protocol));
    // } Testing Methods
    
private:
    using RegisteredEndpoints = std::unordered_map<Network::Endpoint::Identifier, Registration>;
    
    void BindSecurityContext(MessageContext& context) const;

    Node::SharedIdentifier m_spIdentifier;
    IPeerMediator* const m_pPeerMediator;
    
    std::atomic<Security::State> m_authorization;
    mutable std::shared_mutex m_securityMutex;
    std::unique_ptr<Resolver> m_upResolver;
    std::unique_ptr<ISecurityStrategy> m_upSecurityStrategy;

    mutable std::recursive_mutex m_endpointsMutex;
    RegisteredEndpoints m_endpoints;

    mutable std::recursive_mutex m_receiverMutex;
    IMessageSink* m_pEnabledProcessor;
    std::weak_ptr<IMessageSink> m_wpCoreProcessor;
    mutable Statistics m_statistics;
};

//----------------------------------------------------------------------------------------------------------------------
