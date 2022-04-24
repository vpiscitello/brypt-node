//----------------------------------------------------------------------------------------------------------------------
// File: Manager.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Proxy.hpp"
#include "Resolver.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Event/SharedPublisher.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Interfaces/PeerCache.hpp"
#include "Interfaces/PeerMediator.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/tag.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <shared_mutex>
//----------------------------------------------------------------------------------------------------------------------

class IConnectProtocol;
class IPeerObserver;

namespace Awaitable { class TrackingService; }
namespace Node { class ServiceProvider; }

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Manager;

//----------------------------------------------------------------------------------------------------------------------
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Manager final : public IPeerMediator, public IPeerCache
{
public:
    using Predicate = std::function<bool(Peer::Proxy const&)>;
    using ForEachFunction = std::function<CallbackIteration(std::shared_ptr<Peer::Proxy> const)>;

    Manager(Security::Strategy strategy, std::shared_ptr<Node::ServiceProvider> const& spServiceProvider);

    // IPeerMediator {
    virtual void RegisterObserver(IPeerObserver* const observer) override;
    virtual void UnpublishObserver(IPeerObserver* const observer) override;

    virtual OptionalRequest DeclareResolvingPeer(
        Network::RemoteAddress const& address, Node::SharedIdentifier const& spPeerIdentifier = nullptr) override;

    virtual void RescindResolvingPeer(Network::RemoteAddress const& address) override;

    [[nodiscard]] virtual std::shared_ptr<Peer::Proxy> LinkPeer(
        Node::Identifier const& identifier, Network::RemoteAddress const& address) override;

    virtual void OnEndpointRegistered(
        std::shared_ptr<Peer::Proxy> const& spPeerProxy,
        Network::Endpoint::Identifier identifier,
        Network::RemoteAddress const& address) override;

    virtual void OnEndpointWithdrawn(
        std::shared_ptr<Peer::Proxy> const& spPeerProxy,
        Network::Endpoint::Identifier identifier,
        Network::RemoteAddress const& address, 
        WithdrawalCause cause) override;
    // } IPeerMediator

    // IPeerCache {
    virtual bool ForEach(
        IdentifierReadFunction const& callback, Filter filter = Filter::Active) const override;

    [[nodiscard]] virtual std::size_t ActiveCount() const override;
    [[nodiscard]] virtual std::size_t InactiveCount() const override;
    [[nodiscard]] virtual std::size_t ObservedCount() const override;
    [[nodiscard]] virtual std::size_t ResolvingCount() const override;
    // } IPeerCache

    bool ForEach(ForEachFunction const& callback, Filter filter = Filter::Active) const;

    [[nodiscard]] bool Dispatch(
        std::string_view identifier, std::string_view route, Message::Payload&& payload) const;

    [[nodiscard]] std::size_t Notify(
        Message::Destination destination,
        std::string_view route,
        Message::Payload const& payload,
        Predicate const& predicate = {}) const;

    [[nodiscard]] std::optional<Awaitable::TrackerKey> Request(
        std::string_view identifier,
        std::string_view route, 
        Message::Payload&& payload,
        Action::OnResponse const& onResponse,
        Action::OnError const& onError) const;

    [[nodiscard]] std::optional<Awaitable::TrackerKey> Request(
        Message::Destination destination,
        std::string_view route, 
        Message::Payload const& payload,
        Action::OnResponse const& onResponse,
        Action::OnError const& onError,
        Predicate const& predicate = {}) const;

    bool ScheduleDisconnect(std::string_view identifier) const; 
    bool ScheduleDisconnect(Node::Identifier const& identifier) const; 
    std::size_t ScheduleDisconnect(Network::Address const& address) const; 

private:
    struct InternalIndex {};
    struct ExternalIndex {};
    
    using ObserverSet = std::set<IPeerObserver*>;

    using PeerTrackingMap = boost::multi_index_container<
        std::shared_ptr<Proxy>,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<InternalIndex>,
                boost::multi_index::const_mem_fun<
                    Proxy, Node::Internal::Identifier const&, &Proxy::GetIdentifier<Node::Internal::Identifier>>>,
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<ExternalIndex>,
                boost::multi_index::const_mem_fun<
                    Proxy, Node::External::Identifier const&, &Proxy::GetIdentifier<Node::External::Identifier>>>>>;

    using ResolvingPeerMap = std::unordered_map<
        Network::RemoteAddress, std::unique_ptr<Resolver>, Network::AddressHasher<Network::RemoteAddress>>;

    [[nodiscard]] OptionalRequest GenerateShortCircuitRequest(Node::SharedIdentifier const& spPeerIdentifier) const;
    [[nodiscard]] std::shared_ptr<Peer::Proxy> CreatePeer(
        Node::Identifier const& identifier, Network::RemoteAddress const& address);
    void AttachOrCreateExchange(std::shared_ptr<Proxy> const& spProxy, Network::RemoteAddress const& address);

    std::size_t PeerCount(Filter filter) const;

    template<typename FunctionType, typename...Args>
    void NotifyObservers(FunctionType const& function, Args&&...args);

    template<typename FunctionType, typename...Args>
    void NotifyObserversConst(FunctionType const& function, Args&&...args) const;

    Node::SharedIdentifier m_spNodeIdentifier;
    Event::SharedPublisher const m_spEventPublisher;
    Security::Strategy m_strategyType;
    
    mutable std::mutex m_observersMutex;
    ObserverSet m_observers;

    mutable std::shared_mutex m_resolvingMutex;
    ResolvingPeerMap m_resolving;
    
    mutable std::shared_mutex m_peersMutex;
    PeerTrackingMap m_peers;
    std::size_t m_active;
    
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<IConnectProtocol> m_spConnectProtocol;
    std::weak_ptr<Node::ServiceProvider> const m_wpServiceProvider;
};

//----------------------------------------------------------------------------------------------------------------------
