//------------------------------------------------------------------------------------------------
// File: MessageQueue.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageQueue.hpp"
//------------------------------------------------------------------------------------------------
#include <algorithm>
//------------------------------------------------------------------------------------------------

CMessageQueue::CMessageQueue()
	: m_incoming()
	, m_callbacks()
	, m_mutex()
{
}

//------------------------------------------------------------------------------------------------

CMessageQueue::~CMessageQueue()
{
}

//------------------------------------------------------------------------------------------------

bool CMessageQueue::PushOutgoingMessage(
	NodeUtils::NodeIdType id,
	std::string_view message)
{
	NodeUtils::printo("Messages queued for " + std::to_string(id), NodeUtils::PrintType::MQUEUE);

	// Attempt to find a mapped callback for the node in the known callbacks
	// If it exists and has a context attached forward the message to the handler
	if (auto const itr = m_callbacks.find(id); itr != m_callbacks.end()) {
		auto const [context, callback] = itr->second;
		if (context) {
			callback(context, message);
		}
	}

	return true;
}

//------------------------------------------------------------------------------------------------

std::optional<CMessage> CMessageQueue::PopIncomingMessage()
{
	if (m_incoming.empty()) {
		return {};
	}

	CMessage message = m_incoming.front();
	m_incoming.pop();

	NodeUtils::printo(std::to_string(m_incoming.size()) + " left in incoming queue", NodeUtils::PrintType::MQUEUE);

	return message;
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::ForwardMessage(
	NodeUtils::NodeIdType id,
	std::string_view message)
{
	std::scoped_lock lock(m_mutex);
	m_incoming.emplace(message.data());
}

//------------------------------------------------------------------------------------------------

void CMessageQueue::RegisterCallback(
	NodeUtils::NodeIdType id,
	CConnection* context,
	ProcessedMessageCallback callback)
{
	std::scoped_lock lock(m_mutex);

	// If the current ID is already mapped, return as we do not want to 
	// clobber the currently stored context and callback
	if (auto const itr = m_callbacks.find(id); itr != m_callbacks.end()) {
		return;
	}

	m_callbacks[id] = {context, callback};
}

//------------------------------------------------------------------------------------------------