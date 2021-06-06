//----------------------------------------------------------------------------------------------------------------------
// File: MessageSinkStub.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "MessageSinkStub.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "BryptMessage/NetworkMessage.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <mutex>
//----------------------------------------------------------------------------------------------------------------------

MessageSinkStub::MessageSinkStub(Node::SharedIdentifier const& spNodeIdentifier)
	: m_mutex()
	, m_spNodeIdentifier(spNodeIdentifier)
	, m_incoming()
	, m_bReceivedHeartbeatRequest(false)
	, m_bReceivedHeartbeatResponse(false)
	, m_invalidMessageCount(0)
{
	assert(m_spNodeIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

bool MessageSinkStub::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
	MessageContext const& context,
	std::string_view buffer)
{
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = Z85::Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpPeerProxy, context, decoded);
}

//----------------------------------------------------------------------------------------------------------------------

bool MessageSinkStub::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
	MessageContext const& context,
	std::span<std::uint8_t const> buffer)
{
    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer);
    if (!optProtocol) {
        return false;
    }

	// Handle the message based on the message protocol indicated by the message.
    switch (*optProtocol) {
		// In the case of the application protocol, build an application message and add it too 
		// the queue if it is valid. 
        case Message::Protocol::Application: {
			// Build the application message.
			auto const optMessage = ApplicationMessage::Builder()
				.SetMessageContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			// If the message is invalid increase the invalid count and return an error.
			if (!optMessage) {
				++m_invalidMessageCount;
				return false;
			}

			return QueueMessage(wpPeerProxy, *optMessage);
		}
		// In the case of the network protocol, build a network message and process the message.
        case Message::Protocol::Network: {
			auto const optRequest = NetworkMessage::Builder()
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			// If the message is invalid, increase the invalid count and return an error.
			if (!optRequest) {
				++m_invalidMessageCount;
				return false;
			}

			// Process the message dependent on the network message type.
			switch (optRequest->GetMessageType()) {
				// In the case of a heartbeat request, build a heartbeat response and send it to
				// the peer.
				case Message::Network::Type::HeartbeatRequest: {
					// Indicate we have received a heartbeat request for any tests.
					m_bReceivedHeartbeatRequest = true;	

					// Build the heartbeat response.
					auto const optResponse = NetworkMessage::Builder()
						.MakeHeartbeatResponse()
						.SetSource(*m_spNodeIdentifier)
						.SetDestination(optRequest->GetSourceIdentifier())
						.ValidatedBuild();
					assert(optResponse);

					// Obtain the peer and send the heartbeat response.
					if (auto const spPeerProxy = wpPeerProxy.lock(); spPeerProxy) {
						return spPeerProxy->ScheduleSend(context.GetEndpointIdentifier(), optResponse->GetPack());
					} else {
						++m_invalidMessageCount;
						return false;
					}
				}
				case Message::Network::Type::HeartbeatResponse: {
					m_bReceivedHeartbeatResponse = true;	
					return true;
				}
				// All other network messages are unexpected.
				default: ++m_invalidMessageCount; return false;
			}
		}
		// All other message protocols are unexpected.
        case Message::Protocol::Invalid:
		default: ++m_invalidMessageCount; return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<AssociatedMessage> MessageSinkStub::GetNextMessage()
{
	std::scoped_lock lock(m_mutex);
	if (m_incoming.empty()) { return {}; }

	auto const message = std::move(m_incoming.front());
	m_incoming.pop();

	return message;
}

//----------------------------------------------------------------------------------------------------------------------

bool MessageSinkStub::ReceviedHeartbeatRequest() const
{
	std::scoped_lock lock(m_mutex);
	return m_bReceivedHeartbeatRequest;
}

//----------------------------------------------------------------------------------------------------------------------

bool MessageSinkStub::ReceviedHeartbeatResponse() const
{
	std::scoped_lock lock(m_mutex);
	return m_bReceivedHeartbeatResponse;
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t MessageSinkStub::InvalidMessageCount() const
{
	std::scoped_lock lock(m_mutex);
	return m_invalidMessageCount;
}

//----------------------------------------------------------------------------------------------------------------------

bool MessageSinkStub::QueueMessage(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
	ApplicationMessage const& message)
{
	std::scoped_lock lock(m_mutex);
	m_incoming.emplace(AssociatedMessage{ wpPeerProxy, message });
	return true;
}

//----------------------------------------------------------------------------------------------------------------------
