//------------------------------------------------------------------------------------------------
// File: PeerManager.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "PeerManager.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: 
//------------------------------------------------------------------------------------------------
CPeerManager::CPeerManager()
    : m_peersMutex()
    , m_peers()
    , m_observersMutex()
    , m_observers()
{
}

//------------------------------------------------------------------------------------------------

void CPeerManager::RegisterObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.emplace(observer);
}

//------------------------------------------------------------------------------------------------

void CPeerManager::UnpublishObserver(IPeerObserver* const observer)
{
    std::scoped_lock lock(m_observersMutex);
    m_observers.erase(observer);
}

//------------------------------------------------------------------------------------------------

std::shared_ptr<CBryptPeer> CPeerManager::LinkPeer(
    BryptIdentifier::CContainer const& identifier)
{
    std::shared_ptr<CBryptPeer> spBryptPeer = {};

    {
        std::scoped_lock lock(m_peersMutex);
        // If the provided peer has an identifier that matches an already tracked peer, the 
        // tracked peer needs to be returned to the caller. 
        // Otherwise, a new peer needs to be constructed, tracked, and returned to the caller. 
        if(auto const itr = m_peers.find(identifier.GetInternalRepresentation());itr != m_peers.end()) {
            m_peers.modify(itr, [&spBryptPeer](
                std::shared_ptr<CBryptPeer>& spTrackedPeer)
            {
                assert(spTrackedPeer);  // The tracked peer should always exist while in the container. 
                spBryptPeer = spTrackedPeer;
            });
        } else {
            spBryptPeer = std::make_shared<CBryptPeer>(identifier, this);
            m_peers.emplace(spBryptPeer);
        }
    }

    return spBryptPeer;
}

//------------------------------------------------------------------------------------------------

void CPeerManager::DispatchPeerStateChange(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    Endpoints::EndpointIdType identifier,
    Endpoints::TechnologyType technology,
    ConnectionState change)
{
    NotifyObservers(
        &IPeerObserver::HandlePeerStateChange,
        wpBryptPeer, identifier, technology, change);
}

//------------------------------------------------------------------------------------------------

bool CPeerManager::ForEachCachedIdentifier(
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

std::uint32_t CPeerManager::ActivePeerCount() const
{
    return PeerCount(Filter::Active);
}

//-----------------------------------------------------------------------------------------------

std::uint32_t CPeerManager::InactivePeerCount() const
{
    return PeerCount(Filter::Inactive);
}

//-----------------------------------------------------------------------------------------------

std::uint32_t CPeerManager::ObservedPeerCount() const
{
    return PeerCount(Filter::None);
}

//-----------------------------------------------------------------------------------------------

std::uint32_t CPeerManager::PeerCount(Filter filter) const
{
    std::uint32_t count = 0;

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
void CPeerManager::NotifyObservers(FunctionType const& function, Args&&...args)
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

//------------------------------------------------------------------------------------------------

template<typename FunctionType, typename...Args>
void CPeerManager::NotifyObserversConst(FunctionType const& function, Args&&...args) const
{
    for (auto const& observer: m_observers) {
        auto const binder = std::bind(function, observer, std::forward<Args>(args)...);
        binder();
    }
}

//------------------------------------------------------------------------------------------------
