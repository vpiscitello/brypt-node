//----------------------------------------------------------------------------------------------------------------------
// File: PeerManager.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptPeer.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SecurityMediator.hpp"
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
// Description:
//----------------------------------------------------------------------------------------------------------------------
class PeerManager : public IPeerMediator, public IPeerCache
{
public:
    using ForEachPeerFunction = std::function<CallbackIteration(
        std::shared_ptr<BryptPeer> const)>;

    PeerManager(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        Security::Strategy strategy,
        std::shared_ptr<IConnectProtocol> const& spConnectProtocol,
        std::weak_ptr<IMessageSink> const& wpPromotedProcessor = {});

    // IPeerMediator {
    virtual void RegisterObserver(IPeerObserver* const observer) override;
    virtual void UnpublishObserver(IPeerObserver* const observer) override;

    virtual OptionalRequest DeclareResolvingPeer(
        Network::RemoteAddress const& address,
        BryptIdentifier::SharedContainer const& spIdentifier = nullptr) override;

    virtual void UndeclareResolvingPeer(Network::RemoteAddress const& address) override;

    virtual std::shared_ptr<BryptPeer> LinkPeer(
        BryptIdentifier::Container const& identifier,
        Network::RemoteAddress const& address) override;

    virtual void DispatchPeerStateChange(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        ConnectionState change) override;
    // } IPeerMediator

    // IPeerCache {
    virtual bool ForEachCachedIdentifier(
      IdentifierReadFunction const& callback,
      Filter filter = Filter::Active) const override;

    virtual std::size_t ActivePeerCount() const override;
    virtual std::size_t InactivePeerCount() const override;
    virtual std::size_t ObservedPeerCount() const override;

    virtual std::size_t ResolvingPeerCount() const override;
    // } IPeerCache

    bool ForEachPeer(ForEachPeerFunction const& callback, Filter filter = Filter::Active) const;

private:
    using ObserverSet = std::set<IPeerObserver*>;

    using PeerTrackingMap = boost::multi_index_container<
        std::shared_ptr<BryptPeer>,
        boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<
                boost::multi_index::const_mem_fun<
                    BryptPeer,
                    BryptIdentifier::Internal::Type,
                    &BryptPeer::GetInternalIdentifier>>>>;

    using ResolvingPeerMap = std::unordered_map<
        Network::RemoteAddress, std::unique_ptr<SecurityMediator>,
        Network::AddressHasher<Network::RemoteAddress>>;

    std::size_t PeerCount(Filter filter) const;

    template<typename FunctionType, typename...Args>
    void NotifyObservers(FunctionType const& function, Args&&...args);

    template<typename FunctionType, typename...Args>
    void NotifyObserversConst(FunctionType const& function, Args&&...args) const;

    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    Security::Strategy m_strategyType;
    
    mutable std::mutex m_observersMutex;
    ObserverSet m_observers;

    mutable std::shared_mutex m_peersMutex;
    PeerTrackingMap m_peers;

    mutable std::recursive_mutex m_resolvingMutex;
    ResolvingPeerMap m_resolving;
    
    std::shared_ptr<IConnectProtocol> m_spConnectProtocol;
    std::weak_ptr<IMessageSink> const m_wpPromotedProcessor;
};

//----------------------------------------------------------------------------------------------------------------------
