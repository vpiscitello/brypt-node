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

CMessageSinkStub::CMessageSinkStub(BryptIdentifier::SharedContainer const& spBryptIdentifier)
	: m_mutex()
	, m_spBryptIdentifier(spBryptIdentifier)
	, m_incoming()
	, m_bReceivedHeartbeatRequest(false)
	, m_bReceivedHeartbeatResponse(false)
	, m_invalidMessageCount(0)
{
	assert(m_spBryptIdentifier);
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
		// In the case of the application protocol, build an application message and add it too 
		// the queue if it is valid. 
        case Message::Protocol::Application: {
			// Build the application message.
			auto const optMessage = CApplicationMessage::Builder()
				.SetMessageContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			// If the message is invalid increase the invalid count and return an error.
			if (!optMessage) {
				++m_invalidMessageCount;
				return false;
			}

			return QueueMessage(wpBryptPeer, *optMessage);
		}
		// In the case of the network protocol, build a network message and process the message.
        case Message::Protocol::Network: {
			auto const optRequest = CNetworkMessage::Builder()
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
					auto const optResponse = CNetworkMessage::Builder()
						.MakeHeartbeatResponse()
						.SetSource(*m_spBryptIdentifier)
						.SetDestination(optRequest->GetSourceIdentifier())
						.ValidatedBuild();
					assert(optResponse);

					// Obtain the peer and send the heartbeat response.
					if (auto const spBryptPeer = wpBryptPeer.lock(); spBryptPeer) {
						return spBryptPeer->ScheduleSend(
							context.GetEndpointIdentifier(), optResponse->GetPack());
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

//------------------------------------------------------------------------------------------------

std::optional<AssociatedMessage> CMessageSinkStub::GetNextMessage()
{
	std::scoped_lock lock(m_mutex);
	if (m_incoming.empty()) {
		return {};
	}

	auto const message = m_incoming.front();
	m_incoming.pop();

	return message;
}

//------------------------------------------------------------------------------------------------

bool CMessageSinkStub::ReceviedHeartbeatRequest() const
{
	std::scoped_lock lock(m_mutex);
	return m_bReceivedHeartbeatRequest;
}

//------------------------------------------------------------------------------------------------

bool CMessageSinkStub::ReceviedHeartbeatResponse() const
{
	std::scoped_lock lock(m_mutex);
	return m_bReceivedHeartbeatResponse;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageSinkStub::InvalidMessageCount() const
{
	std::scoped_lock lock(m_mutex);
	return m_invalidMessageCount;
}

//------------------------------------------------------------------------------------------------

bool CMessageSinkStub::QueueMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
	CApplicationMessage const& message)
{
	std::scoped_lock lock(m_mutex);
	m_incoming.emplace(AssociatedMessage{ wpBryptPeer, message });
	return true;
}

//------------------------------------------------------------------------------------------------
