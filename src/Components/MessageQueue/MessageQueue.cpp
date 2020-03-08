//------------------------------------------------------------------------------------------------
// File: MessageQueue.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageQueue.hpp"
//------------------------------------------------------------------------------------------------
#include <algorithm>
//------------------------------------------------------------------------------------------------

CMessageQueue::CMessageQueue()
	: m_incomingMutex()
	, m_callbacksMutex()
	, m_incoming()
	, m_callbacks()
{
}

//------------------------------------------------------------------------------------------------

CMessageQueue::~CMessageQueue()
{
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::PushOutgoingMessage(
	NodeUtils::NodeIdType id,
	CMessage const& message)
{
	NodeUtils::printo("Message queued for " + std::to_string(id), NodeUtils::PrintType::MessageQueue);

	std::scoped_lock lock(m_callbacksMutex);
	// Attempt to find a mapped callback for the node in the known callbacks
	// If it exists and has a context attached forward the message to the handler
	if (auto const itr = m_callbacks.find(id); itr != m_callbacks.end()) {
		auto const [context, callback] = itr->second;
		if (context) {
			callback(context, message);
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageQueue::QueuedMessages()
{
	std::shared_lock lock(m_incomingMutex);
	return m_incoming.size();
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageQueue::RegisteredConnections()
{
	std::shared_lock lock(m_callbacksMutex);
	return m_callbacks.size();
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

	NodeUtils::printo(std::to_string(m_incoming.size()) + " left in incoming queue", NodeUtils::PrintType::MessageQueue);

	return message;
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::ForwardMessage(
	NodeUtils::NodeIdType /*id*/,
	CMessage const& message)
{
	std::scoped_lock lock(m_incomingMutex);
	m_incoming.emplace(message);
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::RegisterCallback(
	NodeUtils::NodeIdType id,
	CConnection* context,
	ProcessedMessageCallback callback)
{
	std::scoped_lock lock(m_callbacksMutex);

	// If the current ID is already mapped, return as we do not want to 
	// clobber the currently stored context and callback
	if (auto const itr = m_callbacks.find(id); itr != m_callbacks.end()) {
		return;
	}

	m_callbacks[id] = {context, callback};
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::UnpublishCallback(NodeUtils::NodeIdType id)
{
	std::scoped_lock lock(m_callbacksMutex);
	m_callbacks.erase(id);
}

//------------------------------------------------------------------------------------------------