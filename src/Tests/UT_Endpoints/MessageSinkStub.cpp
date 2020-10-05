//------------------------------------------------------------------------------------------------
// File: MessageSinkStub.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageSinkStub.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
//------------------------------------------------------------------------------------------------
#include <mutex>
//------------------------------------------------------------------------------------------------

CMessageSinkStub::CMessageSinkStub()
	: m_incomingMutex()
	, m_incoming()
{
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageSinkStub::QueuedMessageCount() const
{
	std::shared_lock lock(m_incomingMutex);
	return m_incoming.size();
}

//------------------------------------------------------------------------------------------------

std::optional<AssociatedMessage> CMessageSinkStub::PopIncomingMessage()
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

bool CMessageSinkStub::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CMessageContext const& context,
	std::string_view buffer)
{
    auto const optMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromEncodedPack(buffer)
        .ValidatedBuild();

    if (!optMessage) {
        return false;
    }

	return CollectMessage(wpBryptPeer, *optMessage);
}

//------------------------------------------------------------------------------------------------

bool CMessageSinkStub::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CMessageContext const& context,
	Message::Buffer const& buffer)
{
    auto const optMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromDecodedPack(buffer)
        .ValidatedBuild();

    if (!optMessage) {
        return false;
    }

	return CollectMessage(wpBryptPeer, *optMessage);
}

//------------------------------------------------------------------------------------------------

bool CMessageSinkStub::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CApplicationMessage const& message)
{
	std::scoped_lock lock(m_incomingMutex);
	m_incoming.emplace(AssociatedMessage{ wpBryptPeer, message });
	return true;
}

//------------------------------------------------------------------------------------------------
