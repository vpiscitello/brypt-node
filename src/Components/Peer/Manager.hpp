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
class IMessageSink;
class IPeerObserver;

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
    using ForEachPeerFunction = std::function<CallbackIteration(std::shared_ptr<Peer::Proxy> const)>;

    Manager(
        Node::SharedIdentifier const& spNodeIdentifier,
        Security::Strategy strategy,
        Event::SharedPublisher const& spEventPublisher,
        std::shared_ptr<IConnectProtocol> const& spConnectProtocol,
        std::weak_ptr<IMessageSink> const& wpPromotedProcessor = {});

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
    virtual bool ForEachCachedIdentifier(
        IdentifierReadFunction const& callback, Filter filter = Filter::Active) const override;

    virtual std::size_t ActivePeerCount() const override;
    virtual std::size_t InactivePeerCount() const override;
    virtual std::size_t ObservedPeerCount() const override;

    virtual std::size_t ResolvingPeerCount() const override;
    // } IPeerCache

    bool ForEachPeer(ForEachPeerFunction const& callback, Filter filter = Filter::Active) const;

private:
    using ObserverSet = std::set<IPeerObserver*>;

    using PeerTrackingMap = boost::multi_index_container<
        std::shared_ptr<Proxy>,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::const_mem_fun<
                    Proxy, Node::Internal::Identifier const&, &Proxy::GetIdentifier<Node::Internal::Identifier>>>>>;

    using ResolvingPeerMap = std::unordered_map<
        Network::RemoteAddress, std::unique_ptr<Resolver>, Network::AddressHasher<Network::RemoteAddress>>;

    [[nodiscard]] OptionalRequest GenerateShortCircuitRequest(Node::SharedIdentifier const& spPeerIdentifier) const;
    [[nodiscard]] std::shared_ptr<Peer::Proxy> CreatePeer(
        Node::Identifier const& identifier, Network::RemoteAddress const& address);

    std::size_t PeerCount(Filter filter) const;

    template<typename FunctionType, typename...Args>
    void NotifyObservers(FunctionType const& function, Args&&...args);

    template<typename FunctionType, typename...Args>
    void NotifyObserversConst(FunctionType const& function, Args&&...args) const;

    Node::SharedIdentifier const m_spNodeIdentifier;
    Event::SharedPublisher const m_spEventPublisher;
    Security::Strategy m_strategyType;
    
    mutable std::mutex m_observersMutex;
    ObserverSet m_observers;

    mutable std::shared_mutex m_resolvingMutex;
    ResolvingPeerMap m_resolving;
    
    mutable std::shared_mutex m_peersMutex;
    PeerTrackingMap m_peers;
    
    std::shared_ptr<IConnectProtocol> m_spConnectProtocol;
    std::weak_ptr<IMessageSink> const m_wpPromotedProcessor;
};

//----------------------------------------------------------------------------------------------------------------------
