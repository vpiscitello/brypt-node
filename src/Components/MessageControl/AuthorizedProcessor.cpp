//------------------------------------------------------------------------------------------------
// File: AuthorizedProcessor.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "AuthorizedProcessor.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../BryptMessage/MessageUtils.hpp"
#include "../../BryptMessage/NetworkMessage.hpp"
#include "../../Utilities/Z85.hpp"
//------------------------------------------------------------------------------------------------
#include <mutex>
//------------------------------------------------------------------------------------------------

CAuthorizedProcessor::CAuthorizedProcessor()
	: m_incomingMutex()
	, m_incoming()
{
}

//------------------------------------------------------------------------------------------------

bool CAuthorizedProcessor::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CMessageContext const& context,
	std::string_view buffer)
{
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = Z85::Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpBryptPeer, context, decoded);
}

//------------------------------------------------------------------------------------------------

bool CAuthorizedProcessor::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CMessageContext const& context,
	Message::Buffer const& buffer)
{
    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer);
    if (!optProtocol) {
        return false;
    }

	// Handle the message based on the message protocol indicated by the message.
    switch (*optProtocol) {
        case Message::Protocol::Network: {
			auto const optMessage = CNetworkMessage::Builder()
				.SetMessageContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			if (!optMessage) {
				return false;
			}

			return HandleMessage(wpBryptPeer, *optMessage);
		} 
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
        case Message::Protocol::Invalid:
		default: return false;
    }
}

//------------------------------------------------------------------------------------------------

std::uint32_t CAuthorizedProcessor::QueuedMessageCount() const
{
	std::shared_lock lock(m_incomingMutex);
	return m_incoming.size();
}

//------------------------------------------------------------------------------------------------

std::optional<AssociatedMessage> CAuthorizedProcessor::PopIncomingMessage()
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

bool CAuthorizedProcessor::QueueMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CApplicationMessage const& message)
{
	std::scoped_lock lock(m_incomingMutex);
	m_incoming.emplace(AssociatedMessage{ wpBryptPeer, message });
	return true;
}

//------------------------------------------------------------------------------------------------

bool CAuthorizedProcessor::HandleMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CNetworkMessage const& message)
{
	auto const& optDestination = message.GetDestinationIdentifier();
	if (!optDestination || message.GetDestinationType() != Message::Destination::Node) {
		return false;
	}

	auto const spBryptPeer = wpBryptPeer.lock();
	if (!spBryptPeer) {
		return false;
	}

	Message::Network::Type type = message.GetMessageType();

	CNetworkBuilder::OptionalMessage optResponse;
	switch (type) {
		// Allow heartbeat requests to be processed. 
		case Message::Network::Type::HeartbeatRequest:  {
			optResponse = CNetworkMessage::Builder()
				.SetSource(*optDestination)
				.SetDestination(message.GetSourceIdentifier())
				.MakeHeartbeatResponse()
				.ValidatedBuild();
			assert(optResponse);
		} break;
		// Currently, heartbeat responses are silently dropped from this processor.
		case Message::Network::Type::HeartbeatResponse: return true;
		// Currently, handshake messages are silently dropped from this processor.
		case Message::Network::Type::Handshake: return true;
		default: return false; // What is this?
	}

	// Send the build response to the network message. 
	return spBryptPeer->ScheduleSend(
		message.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
}

//------------------------------------------------------------------------------------------------
