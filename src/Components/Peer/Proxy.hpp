//----------------------------------------------------------------------------------------------------------------------
// File: Proxy.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Action.hpp"
#include "Registration.hpp"
#include "Resolver.hpp"
#include "Statistics.hpp"
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Event/Events.hpp"
#include "Components/Identifier/IdentifierTypes.hpp"
#include "Components/Message/MessageTypes.hpp"
#include "Components/Message/ShareablePack.hpp"
#include "Components/Network/Actions.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/CipherPackage.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Utilities/CallbackIteration.hpp"
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

class NodeState;
class IConnectProtocol;
class IResolutionService;

namespace Awaitable { class TrackingService; }
namespace Network { class Address; }
namespace Node { class ServiceProvider; }
namespace Message { class Context; }
namespace Message::Application { class Builder; }

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Proxy;

template<typename Type>
concept AllowableGetIdentifierType = std::is_same_v<Type, Node::SharedIdentifier> || Node::SupportedIdentifierCast<Type>;

//----------------------------------------------------------------------------------------------------------------------
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Proxy final : public std::enable_shared_from_this<Proxy>, public TokenizedInstance<Proxy>
{
public:
    // Note: An instance of the proxy must be created through the inherited CreateInstance() method. This is to ensure
    // all instances are held within an std::shared_ptr, otherwise shared_from_this() can lead to undefined behavior. 
    Proxy(InstanceToken, Node::Identifier const& identifier, std::shared_ptr<Node::ServiceProvider> const& spProvider);
    ~Proxy();

    template<AllowableGetIdentifierType Type = Node::SharedIdentifier>
    [[nodiscard]] Type const& GetIdentifier() const;

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
    [[nodiscard]] std::optional<Awaitable::TrackerKey> Request(
        Message::Application::Builder& builder, Action::OnResponse const& onResponse, Action::OnError const& onError);
    
    [[nodiscard]] bool ScheduleSend(Message::Application::Builder& builder) const;
    [[nodiscard]] bool ScheduleSend(Network::Endpoint::Identifier identifier, std::string&& message) const;
    [[nodiscard]] bool ScheduleSend(
        Network::Endpoint::Identifier identifier, Message::ShareablePack const& spSharedPack) const;
    // } Message Dispatch Methods

    // Network Association Methods {
    using WithdrawalCause = Event::Message<Event::Type::PeerDisconnected>::Cause;
    using EndpointReader = std::function<CallbackIteration(Registration const&)>;
    using AddressReader = std::function<CallbackIteration(Network::RemoteAddress const&, bool)>;

    void RegisterEndpoint(
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        Network::RemoteAddress const& address = {},
        Network::MessageAction const& scheduler = {},
        Network::DisconnectAction const& disconnector = {});
    void WithdrawEndpoint(Network::Endpoint::Identifier identifier, WithdrawalCause cause);

    void AssociateRemote(Network::RemoteAddress const& remote);

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool IsEndpointRegistered(Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] bool IsEndpointRegistered(Network::Address const& address) const;
    [[nodiscard]] bool IsEndpointRegistered(std::string_view uri) const;
    [[nodiscard]] std::optional<Message::Context> GetMessageContext(
        Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::optional<Network::RemoteAddress> GetRegisteredAddress(
        Network::Endpoint::Identifier identifier) const;
    [[nodiscard]] std::size_t RegisteredEndpointCount() const;

    [[nodiscard]] bool IsRemoteAssociated(Network::RemoteAddress const& remote) const;
    [[nodiscard]] bool IsRemoteAssociated(Network::Protocol protocol, std::string_view address) const;
    [[nodiscard]] bool IsRemoteAssociated(std::string_view uri) const;

    [[nodiscard]] bool IsRemoteConnected(Network::RemoteAddress const& remote) const;
    [[nodiscard]] bool IsRemoteConnected(Network::Protocol protocol, std::string_view address) const;
    [[nodiscard]] bool IsRemoteConnected(std::string_view uri) const;

    bool ForEach(EndpointReader const& reader) const;
    bool ForEach(AddressReader const& reader) const;

    [[nodiscard]] bool ScheduleDisconnect() const;
    // } Network Association Methods

    // Security Methods {
    [[nodiscard]] bool AttachResolver(std::unique_ptr<Resolver>&& upResolver);
    void DetachResolver();
    [[nodiscard]] bool StartExchange(
        Security::ExchangeRole role, std::shared_ptr<Node::ServiceProvider> spServiceProvider);
    [[nodiscard]] Security::State GetAuthorization() const;
    [[nodiscard]] bool IsFlagged() const;
    [[nodiscard]] bool IsAuthorized() const;
    // } Security Methods

    // Testing Methods {
    UT_SupportMethod(void SetResolutionService(std::weak_ptr<IResolutionService> const& wpResolutionService));
    UT_SupportMethod(void SetReceiver(IMessageSink* const pMessageSink));
    UT_SupportMethod(void SetAuthorization(Security::State state));
    UT_SupportMethod(void AttachCipherPackage(std::unique_ptr<Security::CipherPackage>&& upCipherPackage));
    UT_SupportMethod(
        void RegisterSilentEndpoint(
            Network::Endpoint::Identifier identifier,
            Network::Protocol protocol,
            Network::RemoteAddress const& address = {},
            Network::MessageAction const& scheduler = {}));
    UT_SupportMethod(void WithdrawSilentEndpoint(Network::Endpoint::Identifier identifier, Network::Protocol protocol));
    // } Testing Methods
    
private:
    using RegisteredEndpoints = std::unordered_map<Network::Endpoint::Identifier, Registration>;
    using AssociatedAddresses = std::unordered_map<Network::RemoteAddress, bool, Network::AddressHasher<Network::RemoteAddress>>;
    
    [[nodiscard]] RegisteredEndpoints::const_iterator GetOrSetPreferredEndpoint(
        Message::Application::Builder& builder) const; 
    [[nodiscard]] RegisteredEndpoints::const_iterator FetchPreferredEndpoint() const; 

    AssociatedAddresses::const_iterator FindRemote(Network::Protocol protocol, std::string_view address) const;
    AssociatedAddresses::const_iterator FindRemote(std::string_view uri) const;

    void BindSecurityContext(Message::Context& context);

    Node::SharedIdentifier m_spIdentifier;
    std::weak_ptr<IResolutionService> m_wpResolutionService;
    std::weak_ptr<Awaitable::TrackingService> m_wpTrackingService;
    
    std::atomic<Security::State> m_authorization;
    mutable std::shared_mutex m_securityMutex;
    std::unique_ptr<Resolver> m_upResolver;
    std::unique_ptr<Security::CipherPackage> m_upCipherPackage;

    mutable std::recursive_mutex m_endpointsMutex;
    RegisteredEndpoints m_endpoints;

    mutable std::shared_mutex m_associatedMutex;
    AssociatedAddresses m_associated;

    mutable std::recursive_mutex m_receiverMutex;
    IMessageSink* m_pEnabledProcessor;
    std::weak_ptr<IMessageSink> m_wpCoreProcessor;
    mutable Statistics m_statistics;
};

//----------------------------------------------------------------------------------------------------------------------
