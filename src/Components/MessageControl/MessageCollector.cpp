//------------------------------------------------------------------------------------------------
// File: MessageCollector.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageCollector.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../../Message/Message.hpp"
//------------------------------------------------------------------------------------------------
#include <mutex>
//------------------------------------------------------------------------------------------------

CMessageCollector::CMessageCollector()
	: m_incomingMutex()
	, m_incoming()
{
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageCollector::QueuedMessageCount() const
{
	std::shared_lock lock(m_incomingMutex);
	return m_incoming.size();
}

//------------------------------------------------------------------------------------------------

std::optional<AssociatedMessage> CMessageCollector::PopIncomingMessage()
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

void CMessageCollector::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CMessage const& message)
{
	std::scoped_lock lock(m_incomingMutex);
	m_incoming.emplace(AssociatedMessage{ wpBryptPeer, message });
}

//------------------------------------------------------------------------------------------------
