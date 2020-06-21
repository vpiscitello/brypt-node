//------------------------------------------------------------------------------------------------
// File: MessageQueue.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageQueue.hpp"
//------------------------------------------------------------------------------------------------
#include <algorithm>
//------------------------------------------------------------------------------------------------

CRegisteredEndpoint::CRegisteredEndpoint(ProcessedMessageCallback const& callback)
	: m_callback(callback)
	, m_peers()
{
}

//------------------------------------------------------------------------------------------------

ProcessedMessageCallback const& CRegisteredEndpoint::GetCallback() const
{
	return m_callback;
}

//------------------------------------------------------------------------------------------------

void CRegisteredEndpoint::TrackPeer(NodeUtils::NodeIdType id)
{
	m_peers.emplace(id);
}

//------------------------------------------------------------------------------------------------

void CRegisteredEndpoint::UntrackPeer(NodeUtils::NodeIdType id)
{
	m_peers.erase(id);
}

//------------------------------------------------------------------------------------------------

bool CRegisteredEndpoint::IsPeerInGroup(NodeUtils::NodeIdType id) const
{
	if (auto const itr = m_peers.find(id); itr == m_peers.end()) {
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CRegisteredEndpoint::TrackedPeerCount() const
{
	return m_peers.size();
}

//------------------------------------------------------------------------------------------------

CRegisteredEndpoint::ConnectedPeersSet const& CRegisteredEndpoint::GetConnectedPeers() const{
	return m_peers;
}

//------------------------------------------------------------------------------------------------

CMessageQueue::CMessageQueue()
	: m_incomingMutex()
	, m_incoming()
	, m_endpointsMutex()
	, m_endpoints()
	, m_endpointsPeerLookups()
{
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::PushOutgoingMessage(CMessage const& message)
{
	std::scoped_lock lock(m_endpointsMutex);
	// Attempt to find a mapped callback for the node in the known callbacks
	// If it exists and has a context attached forward the message to the handler
	auto messageContext = message.GetMessageContext();
	auto const key = std::make_pair(messageContext.GetEndpointId(), message.GetDestinationId());
	if (auto const itr = m_endpointsPeerLookups.find(key); itr != m_endpointsPeerLookups.end()) {
		auto& [id, lookup] = *itr;
		auto const callback = lookup->second.GetCallback();
		return callback(message);
	}

	return false;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageQueue::QueuedMessageCount() const
{
	std::shared_lock lock(m_incomingMutex);
	return m_incoming.size();
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageQueue::RegisteredEndpointCount() const
{
	std::shared_lock lock(m_endpointsMutex);
	return m_endpoints.size();
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageQueue::TrackedPeerCount() const
{
	std::shared_lock lock(m_endpointsMutex);
	std::uint32_t count = 0;
	for (auto const& [key, registeredEndpoint]: m_endpoints) {
		count += registeredEndpoint.TrackedPeerCount();
	}
	return count;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageQueue::TrackedPeerCount(Endpoints::EndpointIdType id) const
{
	std::shared_lock lock(m_endpointsMutex);
	if (auto const itr = m_endpoints.find(id); itr != m_endpoints.end()) {
		auto const& [key, registeredEndpoint] = *itr;
		return registeredEndpoint.TrackedPeerCount();
	}
	return 0;
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::IsRegistered(Endpoints::EndpointIdType id) const
{
	if (auto const itr = m_endpoints.find(id); itr != m_endpoints.end()) {
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------------------------

std::optional<CMessage> CMessageQueue::PopIncomingMessage()
{
	std::scoped_lock lock(m_incomingMutex);
	if (m_incoming.empty()) {
		return {};
	}

	auto const message = m_incoming.front();
	m_incoming.pop();

	return message;
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::ForwardMessage(CMessage const& message)
{
	std::scoped_lock lock(m_incomingMutex);
	m_incoming.emplace(message);
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::RegisterCallback(Endpoints::EndpointIdType id, ProcessedMessageCallback callback)
{
	std::scoped_lock lock(m_endpointsMutex);
	m_endpoints.try_emplace(
		// Registered Endpoint Key
		id,
		// Registered Endpoint Constructor Arguments
		callback);
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::UnpublishCallback(Endpoints::EndpointIdType id)
{
	std::scoped_lock lock(m_endpointsMutex);
	if (auto const itr = m_endpoints.find(id); itr != m_endpoints.end()) {
		auto const& [key, registeredEndpoint] = *itr;
		// Get the set of tracked peers and erase the associated lookups, the iterator to endpoint
		// tracker will become invalidated after erasure.
		auto const trackedPeers = registeredEndpoint.GetConnectedPeers();
		for (auto const& nodeId : trackedPeers) {
			m_endpointsPeerLookups.erase(std::make_pair(id, nodeId));
		}
		// Erase the endpoint tracker from the map
		m_endpoints.erase(itr);
	}
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::PublishPeerConnection(
	Endpoints::EndpointIdType endpointIdentifier,
	NodeUtils::NodeIdType peerIdentifier)
{
	std::scoped_lock lock(m_endpointsMutex);

	auto const itr = m_endpoints.find(endpointIdentifier); // Get the iterator to the specified endpoint
	// If the associated endpoint is not registered, there is nothing to be done
	if (itr == m_endpoints.end()) {
		return;
	}

	// Attempt to insert the endpoint context lookup for the peer
	m_endpointsPeerLookups.emplace(std::make_pair(endpointIdentifier, peerIdentifier), itr);

	// Add the peer to the list of tracked peers in the registered endpoint tracker
	auto& [key, tracker] = *itr;
	tracker.TrackPeer(peerIdentifier);
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::UnpublishPeerConnection(
	Endpoints::EndpointIdType endpointIdentifier,
	NodeUtils::NodeIdType peerIdentifier)
{
	std::scoped_lock lock(m_endpointsMutex);

	// Attempt to erase the lookup for the peer
	m_endpointsPeerLookups.erase(std::make_pair(endpointIdentifier, peerIdentifier));

	// If the endpoint is actually registered, then untrack the peer in the endpoint tracker
	if (auto const itr = m_endpoints.find(endpointIdentifier); itr != m_endpoints.end()) {
		auto& [key, tracker] = *itr;
		tracker.UntrackPeer(peerIdentifier);
	}
}

//------------------------------------------------------------------------------------------------
