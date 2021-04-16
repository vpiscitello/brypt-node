//----------------------------------------------------------------------------------------------------------------------
// File: AuthorizedProcessor.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "AuthorizedProcessor.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "BryptMessage/NetworkMessage.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <mutex>
//----------------------------------------------------------------------------------------------------------------------

AuthorizedProcessor::AuthorizedProcessor(Node::SharedIdentifier const& spNodeIdentifier)
	: m_spNodeIdentifier(spNodeIdentifier)
    , m_incomingMutex()
	, m_incoming()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy, MessageContext const& context, std::string_view buffer)
{
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = Z85::Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpPeerProxy, context, decoded);
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
	MessageContext const& context,
	std::span<std::uint8_t const> buffer)
{
    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer);
    if (!optProtocol) { return false; }

	// Handle the message based on the message protocol indicated by the message.
    switch (*optProtocol) {
        case Message::Protocol::Network: {
			auto const optMessage = NetworkMessage::Builder()
				.SetMessageContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			if (!optMessage) { return false; }

			return HandleMessage(wpPeerProxy, *optMessage);
		} 
        case Message::Protocol::Application: {
			auto const optMessage = ApplicationMessage::Builder()
				.SetMessageContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			if (!optMessage) { return false; }

			return QueueMessage(wpPeerProxy, *optMessage);
		}
        case Message::Protocol::Invalid:
		default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t AuthorizedProcessor::QueuedMessageCount() const
{
	std::shared_lock lock(m_incomingMutex);
	return m_incoming.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<AssociatedMessage> AuthorizedProcessor::GetNextMessage()
{
	std::scoped_lock lock(m_incomingMutex);
	if (m_incoming.empty()) { return {}; }

	auto const message = m_incoming.front();
	m_incoming.pop();

	return message;
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::QueueMessage(std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage const& message)
{
	std::scoped_lock lock(m_incomingMutex);
	m_incoming.emplace(AssociatedMessage{ wpPeerProxy, message });
	return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::HandleMessage(std::weak_ptr<Peer::Proxy> const& wpPeerProxy, NetworkMessage const& message)
{
    // Currently, there are no network messages that are sent network wide. 
    if (message.GetDestinationType() != Message::Destination::Node) { return false; }

	auto const& optDestination = message.GetDestinationIdentifier();

    // If the network message does not have a destination and is not a handshake message
    // then there is currently no possible message we expect given this context. 
    // If it is a handshake message without a destination identifier, it is assumed that 
    // we woke up and connected to the peer while the peer was actively trying to 
    // connect while this node was offline. 
	if (!optDestination && message.GetMessageType() != Message::Network::Type::Handshake) {
        return false;
	}

    // Currently, messages not destined for this node are note accepted. 
    if (optDestination && *optDestination != *m_spNodeIdentifier) { return false; }

	auto const spPeerProxy = wpPeerProxy.lock();
	if (!spPeerProxy) { return false; }

	Message::Network::Type type = message.GetMessageType();

	NetworkBuilder::OptionalMessage optResponse;
	switch (type) {
		// Allow heartbeat requests to be processed. 
		case Message::Network::Type::HeartbeatRequest:  {
			optResponse = NetworkMessage::Builder()
				.SetSource(*m_spNodeIdentifier)
				.SetDestination(message.GetSourceIdentifier())
				.MakeHeartbeatResponse()
				.ValidatedBuild();
			assert(optResponse);
		} break;
		// Currently, heartbeat responses are silently dropped from this processor.
		case Message::Network::Type::HeartbeatResponse: return true;
		// Currently, handshake requests are responded with a heartbeat request to indicate
        // a valid session has already been established. 
		case Message::Network::Type::Handshake: {
			optResponse = NetworkMessage::Builder()
				.SetSource(*m_spNodeIdentifier)
				.SetDestination(message.GetSourceIdentifier())
				.MakeHeartbeatRequest()
				.ValidatedBuild();
			assert(optResponse);
        } break;
		default: return false; // What is this?
	}

	// Send the build response to the network message. 
	return spPeerProxy->ScheduleSend(
		message.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
}

//----------------------------------------------------------------------------------------------------------------------
