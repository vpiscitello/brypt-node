//----------------------------------------------------------------------------------------------------------------------
// File: Manager.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Manager.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/NetworkMessage.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/PeerObserver.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: 
//----------------------------------------------------------------------------------------------------------------------
Peer::Manager::Manager(
    Node::SharedIdentifier const& spNodeIdentifier,
    Security::Strategy strategy,
    std::shared_ptr<IConnectProtocol> const& spConnectProtocol,
    std::weak_ptr<IMessageSink> const& wpPromotedProcessor)
    : m_spNodeIdentifier(spNodeIdentifier)
    , m_strategyType(strategy)
    , m_observersMutex()
    , m_observers()
    , m_peersMutex()
    , m_peers()
    , m_resolvingMutex()
    , m_resolving()
    , m_spConnectProtocol(spConnectProtocol)
    , m_wpPromotedProcessor(wpPromotedProcessor)
{
    assert(m_strategyType != Security::Strategy::Invalid);
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Manager::RegisterObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.emplace(observer);
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Manager::UnpublishObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.erase(observer);
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Manager::OptionalRequest Peer::Manager::DeclareResolvingPeer(
    Network::RemoteAddress const& address, Node::SharedIdentifier const& spPeerIdentifier)
{
    std::scoped_lock resolvingLock(m_resolvingMutex);

    // Disallow endpoints from connecting to the same uri. If an endpoint has connection retry logic, it should store
    // the connection request message. However, there exists a race  condition when the peer wakes up while the endpoint
    // is still not sure a peer exists at that particular uri. In this case the peer may send a bootstrap request causing 
    // the endpoint to check if we are currently resolving that uri. 
    if (auto const itr = m_resolving.find(address); itr != m_resolving.end()) { return {}; }

    // If the we are provided an identifier for the peer, prefer short circuiting the exchange and send a hearbeat 
    // request to instantiate the endpoint's connection. Otherwise, a new security mediator needs to be created and an 
    // exchange to be intialized. 
    if (spPeerIdentifier) {
        assert(spPeerIdentifier->IsValid());
        std::scoped_lock peersLock(m_peersMutex);
        // If the peer is not currently tracked, a exchange short circuit message cannot be generated. 
        // Otherwise, we should fallthrough to generate an exchange handler. 
        if(auto const itr = m_peers.find(spPeerIdentifier->GetInternalValue()); itr == m_peers.end()) { return {}; }
        // Generate the heartbeat request.
        auto const optRequest = NetworkMessage::Builder()
            .SetSource(*m_spNodeIdentifier)
            .SetDestination(*spPeerIdentifier)
            .MakeHeartbeatRequest()
            .ValidatedBuild();
        assert(optRequest);

        return optRequest->GetPack();
    }
    
    auto upSecurityMediator = std::make_unique<Security::Mediator>(
        m_spNodeIdentifier, Security::Context::Unique, m_wpPromotedProcessor);

    auto const optRequest = upSecurityMediator->SetupExchangeInitiator(m_strategyType, m_spConnectProtocol);
    assert(optRequest);
    
    // Store the SecurityStrategy such that when the endpoint links the peer it can be attached
    // to the full peer proxy
    if (optRequest) { m_resolving[address] = std::move(upSecurityMediator); }

    // Return the ticket number and the initial connection message
    return optRequest;  
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Manager::UndeclareResolvingPeer(Network::RemoteAddress const& address)
{
    std::scoped_lock lock(m_resolvingMutex);
    [[maybe_unused]] std::size_t const result = m_resolving.erase(address);
    assert(result == 1); // This function should only be called if the peer has been declared. 
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> Peer::Manager::LinkPeer(
    Node::Identifier const& identifier, Network::RemoteAddress const& address)
{
    std::shared_ptr<Peer::Proxy> spPeerProxy = {};
    std::scoped_lock peersLock(m_peersMutex);
    // If the provided peer has an identifier that matches an already tracked peer, the tracked peer needs to be 
    // returned to the caller. 
    // Otherwise, a new peer needs to be constructed, tracked, and returned to the caller. 
    if(auto const itr = m_peers.find(identifier.GetInternalValue()); itr != m_peers.end()) {
        m_resolving.erase(address); // Ensure the provided address is not marked as resolving.
        m_peers.modify(itr, [&spPeerProxy](std::shared_ptr<Peer::Proxy>& spTrackedPeer)
            { assert(spTrackedPeer);  spPeerProxy = spTrackedPeer; });
    } else {
        // Make a peer proxy that can be shared with the endpoint. 
        spPeerProxy = std::make_shared<Peer::Proxy>(identifier, this);

        // If the endpoint has a resolving ticket it means that we initiated the connection and we need
        // to move the SecurityStrategy out of the resolving map and give it to the SecurityMediator. 
        // Otherwise, it is assumed are accepting the connection and we need to make an accepting strategy.
        std::unique_ptr<Security::Mediator> upSecurityMediator;
        if (auto itr = m_resolving.find(address); itr != m_resolving.end()) {
            std::scoped_lock resolvingLock(m_resolvingMutex);
            upSecurityMediator = std::move(itr->second);
            m_resolving.erase(itr);
        } else {
            upSecurityMediator = std::make_unique<Security::Mediator>(
                m_spNodeIdentifier, Security::Context::Unique, m_wpPromotedProcessor);

            bool const success = upSecurityMediator->SetupExchangeAcceptor(m_strategyType);
            if (!success) {  return {}; }
        }
        
        spPeerProxy->AttachSecurityMediator(std::move(upSecurityMediator));

        m_peers.emplace(spPeerProxy);
    }

    return spPeerProxy;
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Manager::DispatchPeerStateChange(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    ConnectionState change)
{
    NotifyObservers(&IPeerObserver::HandlePeerStateChange, wpPeerProxy, identifier, protocol, change);
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Manager::ForEachCachedIdentifier(IdentifierReadFunction const& callback, Filter filter) const
{
    std::shared_lock lock(m_peersMutex);
    for (auto const& spPeerProxy: m_peers) {
        bool isIncluded = true;
        switch (filter) {
            case Filter::Active: { isIncluded = spPeerProxy->IsActive(); } break;
            case Filter::Inactive: { isIncluded = !spPeerProxy->IsActive(); } break;
            case Filter::None: { isIncluded = true; } break;
            default: assert(false); // What is this?
        }

        if (isIncluded) {
            if (callback(spPeerProxy->GetNodeIdentifier()) != CallbackIteration::Continue) { break; }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::Manager::ActivePeerCount() const
{
    return PeerCount(Filter::Active);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::Manager::InactivePeerCount() const
{
    return PeerCount(Filter::Inactive);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::Manager::ObservedPeerCount() const
{
    return PeerCount(Filter::None);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::Manager::ResolvingPeerCount() const
{
    std::scoped_lock lock(m_resolvingMutex);
    return m_resolving.size();
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Manager::ForEachPeer(ForEachPeerFunction const& callback, Filter filter) const
{
    std::shared_lock lock(m_peersMutex);
    for (auto const& spPeerProxy: m_peers) {
        bool isIncluded = true;
        switch (filter) {
            case Filter::Active: { isIncluded = spPeerProxy->IsActive(); } break;
            case Filter::Inactive: { isIncluded = !spPeerProxy->IsActive(); } break;
            case Filter::None: { isIncluded = true; } break;
            default: assert(false); // What is this?
        }

        if (isIncluded) {
            if (callback(spPeerProxy) != CallbackIteration::Continue) { break; }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Peer::Manager::PeerCount(Filter filter) const
{
    std::size_t count = 0;

    std::shared_lock lock(m_peersMutex);
    for (auto const& spPeerProxy: m_peers) {
        bool isIncluded = true;
        switch (filter) {
            case Filter::Active: { isIncluded = spPeerProxy->IsActive(); } break;
            case Filter::Inactive: { isIncluded = !spPeerProxy->IsActive(); } break;
            case Filter::None: { isIncluded = true; } break;
            default: assert(false); // What is this?
        }
        
        if (isIncluded) { ++count; }
    }

    return count;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename FunctionType, typename...Args>
void Peer::Manager::NotifyObservers(FunctionType const& function, Args&&...args)
{
    for (auto itr = m_observers.cbegin(); itr != m_observers.cend();) {
        auto const& observer = *itr; // Get a refernce to the current observer pointer
        // If the observer is no longer valid erase the dangling pointer from the set
        // Otherwise, send the observer the notification
        if (!observer) {
            itr = m_observers.erase(itr);
        } else {
            auto const binder = std::bind(function, observer, std::forward<Args>(args)...);
            binder();
            ++itr;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<typename FunctionType, typename...Args>
void Peer::Manager::NotifyObserversConst(FunctionType const& function, Args&&...args) const
{
    for (auto const& observer: m_observers) {
        auto const binder = std::bind(function, observer, std::forward<Args>(args)...);
        binder();
    }
}

//----------------------------------------------------------------------------------------------------------------------
