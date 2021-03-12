//----------------------------------------------------------------------------------------------------------------------
// File: PeerManager.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "PeerManager.hpp"
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
PeerManager::PeerManager(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    Security::Strategy strategy,
    std::shared_ptr<IConnectProtocol> const& spConnectProtocol,
    std::weak_ptr<IMessageSink> const& wpPromotedProcessor)
    : m_spBryptIdentifier(spBryptIdentifier)
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
    if (m_strategyType == Security::Strategy::Invalid) {
        throw std::runtime_error("Peer Manager was not provided a valid security strategy type!");
    }
}

//----------------------------------------------------------------------------------------------------------------------

void PeerManager::RegisterObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.emplace(observer);
}

//----------------------------------------------------------------------------------------------------------------------

void PeerManager::UnpublishObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.erase(observer);
}

//----------------------------------------------------------------------------------------------------------------------

PeerManager::OptionalRequest PeerManager::DeclareResolvingPeer(
    Network::RemoteAddress const& address,
    BryptIdentifier::SharedContainer const& spPeerIdentifier)
{
    std::scoped_lock resolvingLock(m_resolvingMutex);

    // Disallow endpoints from connecting to the same uri. If an endpoint has connection retry
    // logic, it should store the connection request message. However, there exists a race 
    // condition when the peer wakes up while the endpoint is still not sure a peer exists 
    // at that particular uri. In this case the peer may send a bootstrap request causing the 
    // endpoint to check if we are currently resolving that uri. 
    if (auto const itr = m_resolving.find(address); itr != m_resolving.end()) { return {}; }

    // If the we are provided an identifier for the peer, prefer short circuiting the exchange 
    // and send a hearbeat request to instantiate the endpoint's connection. Otherwise, a new
    // security mediator needs to be created and an exchange to be intialized. 
    if (spPeerIdentifier) {
        assert(spPeerIdentifier->IsValid());
        std::scoped_lock peersLock(m_peersMutex);
        // If the peer is not currently tracked, a exchange short circuit message cannot be generated. 
        // Otherwise, we should fallthrough to generate an exchange handler. 
        if(auto const itr = m_peers.find(spPeerIdentifier->GetInternalRepresentation());
            itr != m_peers.end()) {
            // Generate the heartbeat request.
            auto const optRequest = NetworkMessage::Builder()
                .SetSource(*m_spBryptIdentifier)
                .SetDestination(*spPeerIdentifier)
                .MakeHeartbeatRequest()
                .ValidatedBuild();
            assert(optRequest);

            return optRequest->GetPack();
        }
    } else {
        auto upSecurityMediator = std::make_unique<SecurityMediator>(
        m_spBryptIdentifier, Security::Context::Unique, m_wpPromotedProcessor);

        auto const optRequest = upSecurityMediator->SetupExchangeInitiator(
            m_strategyType, m_spConnectProtocol);
        assert(optRequest);
        
        // Store the SecurityStrategy such that when the endpoint links the peer it can be attached
        // to the full BryptPeer
        if (optRequest) { m_resolving[address] = std::move(upSecurityMediator); }

        // Return the ticket number and the initial connection message
        return optRequest;  
    }

    assert(false); // What condition causes both generators to fallthrough? 
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

void PeerManager::UndeclareResolvingPeer(Network::RemoteAddress const& address)
{
    std::scoped_lock lock(m_resolvingMutex);
    [[maybe_unused]] std::size_t const result = m_resolving.erase(address);
    assert(result == 1); // This function should only be called if the peer has been declared. 
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<BryptPeer> PeerManager::LinkPeer(
    BryptIdentifier::Container const& identifier,
    Network::RemoteAddress const& address)
{
    std::shared_ptr<BryptPeer> spBryptPeer = {};

    {
        std::scoped_lock peersLock(m_peersMutex);
        // If the provided peer has an identifier that matches an already tracked peer, the 
        // tracked peer needs to be returned to the caller. 
        // Otherwise, a new peer needs to be constructed, tracked, and returned to the caller. 
        if(auto const itr = m_peers.find(identifier.GetInternalRepresentation()); itr != m_peers.end()) {
            m_resolving.erase(address); // Ensure the provided address is not marked as resolving.
            m_peers.modify(itr, [&spBryptPeer](
                std::shared_ptr<BryptPeer>& spTrackedPeer)
            {
                assert(spTrackedPeer);  // The tracked peer should always exist while in the container. 
                spBryptPeer = spTrackedPeer;
            });
        } else {
            // Make BryptPeer that can be shared with the endpoint. 
            spBryptPeer = std::make_shared<BryptPeer>(identifier, this);

            // If the endpoint has a resolving ticket it means that we initiated the connection and we need
            // to move the SecurityStrategy out of the resolving map and give it to the SecurityMediator. 
            // Otherwise, it is assumed are accepting the connection and we need to make an accepting strategy.
            std::unique_ptr<SecurityMediator> upSecurityMediator;
            if (auto itr = m_resolving.find(address); itr != m_resolving.end()) {
                std::scoped_lock resolvingLock(m_resolvingMutex);
                upSecurityMediator = std::move(itr->second);
                m_resolving.erase(itr);
            } else {
                upSecurityMediator = std::make_unique<SecurityMediator>(
                    m_spBryptIdentifier, Security::Context::Unique, m_wpPromotedProcessor);

                bool const success = upSecurityMediator->SetupExchangeAcceptor(m_strategyType);
                if (!success) {  return {}; }
            }
            
            spBryptPeer->AttachSecurityMediator(std::move(upSecurityMediator));

            m_peers.emplace(spBryptPeer);
        }
    }

    return spBryptPeer;
}

//----------------------------------------------------------------------------------------------------------------------

void PeerManager::DispatchPeerStateChange(
    std::weak_ptr<BryptPeer> const& wpBryptPeer,
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    ConnectionState change)
{
    NotifyObservers(
        &IPeerObserver::HandlePeerStateChange,
        wpBryptPeer, identifier, protocol, change);
}

//----------------------------------------------------------------------------------------------------------------------

bool PeerManager::ForEachCachedIdentifier(
    IdentifierReadFunction const& callback, Filter filter) const
{
    std::shared_lock lock(m_peersMutex);
    for (auto const& spBryptPeer: m_peers) {
        bool bShouldProvideIdentifier = true;
        switch (filter) {
            case Filter::Active: {
                bShouldProvideIdentifier = spBryptPeer->IsActive();
            } break;
            case Filter::Inactive: {
                bShouldProvideIdentifier = !spBryptPeer->IsActive();
            } break;
            case Filter::None: {
                bShouldProvideIdentifier = true;
            } break;
            default: assert(false); // What is this?
        }

        if (bShouldProvideIdentifier) {
            if (callback(spBryptPeer->GetBryptIdentifier()) != CallbackIteration::Continue) {
                break;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------------------------

std::size_t PeerManager::ActivePeerCount() const
{
    return PeerCount(Filter::Active);
}

//-----------------------------------------------------------------------------------------------

std::size_t PeerManager::InactivePeerCount() const
{
    return PeerCount(Filter::Inactive);
}

//-----------------------------------------------------------------------------------------------

std::size_t PeerManager::ObservedPeerCount() const
{
    return PeerCount(Filter::None);
}

//-----------------------------------------------------------------------------------------------

std::size_t PeerManager::ResolvingPeerCount() const
{
    std::scoped_lock lock(m_resolvingMutex);
    return m_resolving.size();
}

//-----------------------------------------------------------------------------------------------

bool PeerManager::ForEachPeer(ForEachPeerFunction const& callback, Filter filter) const
{
    std::shared_lock lock(m_peersMutex);
    for (auto const& spBryptPeer: m_peers) {
        bool bShouldProvideIdentifier = true;
        switch (filter) {
            case Filter::Active: {
                bShouldProvideIdentifier = spBryptPeer->IsActive();
            } break;
            case Filter::Inactive: {
                bShouldProvideIdentifier = !spBryptPeer->IsActive();
            } break;
            case Filter::None: {
                bShouldProvideIdentifier = true;
            } break;
            default: assert(false); // What is this?
        }

        if (bShouldProvideIdentifier) {
            if (callback(spBryptPeer) != CallbackIteration::Continue) {
                break;
            }
        }
    }

    return true;
}

//-----------------------------------------------------------------------------------------------

std::size_t PeerManager::PeerCount(Filter filter) const
{
    std::size_t count = 0;

    std::shared_lock lock(m_peersMutex);
    for (auto const& spBryptPeer: m_peers) {
        bool bShouldCountPeer = true;
        switch (filter) {
            case Filter::Active: {
                bShouldCountPeer = spBryptPeer->IsActive();
            } break;
            case Filter::Inactive: {
                bShouldCountPeer = !spBryptPeer->IsActive();
            } break;
            case Filter::None: {
                bShouldCountPeer = true;
            } break;
            default: assert(false); // What is this?
        }
        
        if (bShouldCountPeer) {
            ++count;
        }
    }

    return count;
}

//-----------------------------------------------------------------------------------------------

template<typename FunctionType, typename...Args>
void PeerManager::NotifyObservers(FunctionType const& function, Args&&...args)
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
void PeerManager::NotifyObserversConst(FunctionType const& function, Args&&...args) const
{
    for (auto const& observer: m_observers) {
        auto const binder = std::bind(function, observer, std::forward<Args>(args)...);
        binder();
    }
}

//----------------------------------------------------------------------------------------------------------------------
