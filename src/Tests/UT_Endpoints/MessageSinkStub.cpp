//------------------------------------------------------------------------------------------------
// File: MessageSinkStub.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageSinkStub.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../BryptMessage/MessageUtils.hpp"
#include "../../BryptMessage/NetworkMessage.hpp"
#include "../../BryptMessage/PackUtils.hpp"
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

bool CMessageSinkStub::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CMessageContext const& context,
	std::string_view buffer)
{
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = PackUtils::Z85Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpBryptPeer, context, decoded);
}

//------------------------------------------------------------------------------------------------

bool CMessageSinkStub::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CMessageContext const& context,
	Message::Buffer const& buffer)
{
    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer.begin(), buffer.end());
    if (!optProtocol) {
        return false;
    }

	// Handle the message based on the message protocol indicated by the message.
    switch (*optProtocol) {
        case Message::Protocol::Application: {
			auto const optMessage = CApplicationMessage::Builder()
				.SetMessageContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			if (!optMessage) {
				return false;
			}

			return QueueMessage(wpBryptPeer, *optMessage);
		}
        case Message::Protocol::Network:
        case Message::Protocol::Invalid:
		default: return false;
    }
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

bool CMessageSinkStub::QueueMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CApplicationMessage const& message)
{
	std::scoped_lock lock(m_incomingMutex);
	m_incoming.emplace(AssociatedMessage{ wpBryptPeer, message });
	return true;
}

//------------------------------------------------------------------------------------------------
