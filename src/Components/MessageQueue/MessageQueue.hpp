//------------------------------------------------------------------------------------------------
// File: MessageQueue.cpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Message/Message.hpp"
//------------------------------------------------------------------------------------------------
#include <atomic>
#include <boost/functional/hash.hpp>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
//------------------------------------------------------------------------------------------------

class CRegisteredEndpoint {
public:
    using ConnectedPeersSet = std::unordered_set<BryptIdentifier::CContainer, BryptIdentifier::Hasher>;

    explicit CRegisteredEndpoint(ProcessedMessageCallback const& callback);

    ProcessedMessageCallback const& GetCallback() const;

    void TrackPeer(BryptIdentifier::CContainer const& identifier);
    void UntrackPeer(BryptIdentifier::CContainer const& identifier);
    bool IsPeerInGroup(BryptIdentifier::CContainer const& identifier) const;
    std::uint32_t TrackedPeerCount() const;

    ConnectedPeersSet const& GetConnectedPeers() const;

private: 
    ProcessedMessageCallback const m_callback;

    std::shared_mutex m_peersMutex;
    ConnectedPeersSet m_peers;
};

//------------------------------------------------------------------------------------------------

class CMessageQueue : public IMessageSink {
public:
    CMessageQueue();
    
    bool PushOutgoingMessage(CMessage const& message);
    std::optional<CMessage> PopIncomingMessage();
    
    std::uint32_t QueuedMessageCount() const;
    std::uint32_t RegisteredEndpointCount() const;
    std::uint32_t TrackedPeerCount() const;
    std::uint32_t TrackedPeerCount(Endpoints::EndpointIdType id) const;
    bool IsRegistered(Endpoints::EndpointIdType id) const;

    // IMessageSink {
    virtual void ForwardMessage(CMessage const& message) override;

    virtual void RegisterCallback(Endpoints::EndpointIdType id, ProcessedMessageCallback callback) override;
    virtual void UnpublishCallback(Endpoints::EndpointIdType id) override;

    virtual void PublishPeerConnection(
        Endpoints::EndpointIdType endpointIdentifier,
        BryptIdentifier::CContainer const& peerIdentifier) override;
    virtual void UnpublishPeerConnection(
        Endpoints::EndpointIdType endpointIdentifier,
        BryptIdentifier::CContainer const& peerIdentifier) override;
    // }IMessageSink

private:
    using RegisteredEndpointMap = std::unordered_map<Endpoints::EndpointIdType, CRegisteredEndpoint>;
    using RegisteredEndpointMapIterator = RegisteredEndpointMap::iterator;
    using PeerContextKey = std::pair<Endpoints::EndpointIdType, BryptIdentifier::InternalType>;
    using PeerContextLookupMap = std::unordered_map<PeerContextKey, RegisteredEndpointMapIterator, boost::hash<PeerContextKey>>;

    mutable std::shared_mutex m_incomingMutex;
    std::queue<CMessage> m_incoming;

    mutable std::shared_mutex m_endpointsMutex;
    RegisteredEndpointMap m_endpoints;
    PeerContextLookupMap m_endpointsPeerLookups;
};

//------------------------------------------------------------------------------------------------