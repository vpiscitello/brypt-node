//----------------------------------------------------------------------------------------------------------------------
// File: AuthorizedProcessor.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "AuthorizedProcessor.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "BryptMessage/PlatformMessage.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <mutex>
//----------------------------------------------------------------------------------------------------------------------

AuthorizedProcessor::AuthorizedProcessor(
	Node::SharedIdentifier const& spNodeIdentifier,
	Handler::Map const& handlers,
	std::shared_ptr<Scheduler::Registrar> const& spRegistrar)
    : m_spDelegate()
	, m_spNodeIdentifier(spNodeIdentifier)
    , m_incomingMutex()
	, m_incoming()
{
	m_spDelegate = spRegistrar->Register<AuthorizedProcessor>([this, &handlers] () -> std::size_t {
		assert(( std::scoped_lock{ m_incomingMutex }, !m_incoming.empty() ));
		if (auto const optMessage = GetNextMessage(); optMessage) {
			auto& [spPeerProxy, message] = *optMessage;



			
			// if (auto const itr = handlers.find(message.GetCommand()); itr != handlers.end()) {
			// 	auto const& [type, handler] = *itr;
			// 	handler->HandleMessage(*optMessage);
			// }
			return 1; // Provide the number of tasks executed to the scheduler. 
		}
		return 0; // Indicate that we were enable to execute a task this cycle. 
    }); 

    assert(m_spDelegate);

	// Ensure that when a message is processed it has access to the latest bootstrap cache state. 
	m_spDelegate->Depends<BootstrapService>(); 
}

//----------------------------------------------------------------------------------------------------------------------

AuthorizedProcessor::~AuthorizedProcessor()
{
	m_spDelegate->Delist();
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy, Message::Context const& context, std::string_view buffer)
{
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = Z85::Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpPeerProxy, context, decoded);
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy, Message::Context const& context, std::span<std::uint8_t const> buffer)
{
    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer);
    if (!optProtocol) { return false; }

	// Handle the message based on the message protocol indicated by the message.
    switch (*optProtocol) {
        case Message::Protocol::Platform: {
			auto const optMessage = Message::Platform::Parcel::GetBuilder()
				.SetContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			if (!optMessage) { return false; }

			return OnMessageCollected(wpPeerProxy, *optMessage);
		} 
        case Message::Protocol::Application: {
			auto optMessage = Message::Application::Parcel::GetBuilder()
				.SetContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			if (!optMessage) { return false; }

			return OnMessageCollected(wpPeerProxy, std::move(*optMessage));
		}
        case Message::Protocol::Invalid:
		default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t AuthorizedProcessor::MessageCount() const
{
	std::shared_lock lock(m_incomingMutex);
	return m_incoming.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<AssociatedMessage> AuthorizedProcessor::GetNextMessage()
{
	std::scoped_lock lock(m_incomingMutex);
	if (m_incoming.empty()) { return {}; }
	auto const message = std::move(m_incoming.front());
	m_incoming.pop();
	return message;
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::OnMessageCollected(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy, Message::Application::Parcel&& message)
{
	std::scoped_lock lock(m_incomingMutex);
	m_incoming.emplace(AssociatedMessage{ wpPeerProxy, std::move(message) });
	m_spDelegate->OnTaskAvailable();
	return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::OnMessageCollected(
	std::weak_ptr<Peer::Proxy> const& wpPeerProxy, Message::Platform::Parcel const& message)
{
    // Currently, there are no network messages that are sent network wide. 
    if (message.GetDestinationType() != Message::Destination::Node) { return false; }

	auto const& optDestination = message.GetDestination();

    // If the network message does not have a destination and is not a handshake message then there is currently no 
	// possible message we expect given this context. If it is a handshake message without a destination identifier,
	//  it is assumed that we woke up and connected to the peer while the peer was actively trying to  connect while 
	// this node was offline. 
	if (!optDestination && message.GetType() != Message::Platform::ParcelType::Handshake) { return false; }

    // Currently, messages not destined for this node are note accepted. 
    if (optDestination && *optDestination != *m_spNodeIdentifier) { return false; }

	auto const spPeerProxy = wpPeerProxy.lock();
	if (!spPeerProxy) { return false; }

	Message::Platform::ParcelType type = message.GetType();

	Message::Platform::Builder::OptionalParcel optResponse;
	switch (type) {
		// Allow heartbeat requests to be processed. 
		case Message::Platform::ParcelType::HeartbeatRequest:  {
			optResponse = Message::Platform::Parcel::GetBuilder()
				.SetSource(*m_spNodeIdentifier)
				.SetDestination(message.GetSource())
				.MakeHeartbeatResponse()
				.ValidatedBuild();
			assert(optResponse);
		} break;
		// Currently, heartbeat responses are silently dropped from this processor.
		case Message::Platform::ParcelType::HeartbeatResponse: return true;
		// Currently, handshake requests are responded with a heartbeat request to indicate a valid session has already
		//  been established. 
		case Message::Platform::ParcelType::Handshake: {
			optResponse = Message::Platform::Parcel::GetBuilder()
				.SetSource(*m_spNodeIdentifier)
				.SetDestination(message.GetSource())
				.MakeHeartbeatRequest()
				.ValidatedBuild();
			assert(optResponse);
        } break;
		default: return false; // What is this?
	}

	// Send the build response to the network message. 
	return spPeerProxy->ScheduleSend(message.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
}

//----------------------------------------------------------------------------------------------------------------------
