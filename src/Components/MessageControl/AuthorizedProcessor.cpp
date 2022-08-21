//----------------------------------------------------------------------------------------------------------------------
// File: AuthorizedProcessor.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "AuthorizedProcessor.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "BryptMessage/PlatformMessage.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Delegate.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/State/NodeState.hpp"
#include "Components/Route/Router.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <mutex>
//----------------------------------------------------------------------------------------------------------------------

AuthorizedProcessor::AuthorizedProcessor(
	std::shared_ptr<Scheduler::Registrar> const& spRegistrar,
	std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
    : m_logger(spdlog::get(Logger::Name::Core.data()))
	, m_spDelegate()
	, m_spNodeIdentifier()
	, m_spRouter(spServiceProvider->Fetch<Route::Router>())
	, m_spTrackingService(spServiceProvider->Fetch<Awaitable::TrackingService>())
	, m_wpServiceProvider(spServiceProvider)
    , m_mutex()
	, m_incoming()
{
	if (auto const spNodeState = spServiceProvider->Fetch<NodeState>().lock(); spNodeState) {
		m_spNodeIdentifier = spNodeState->GetNodeIdentifier();
	}
	assert(m_spNodeIdentifier);
	assert(m_spRouter);
	assert(m_spTrackingService);

	m_spDelegate = spRegistrar->Register<AuthorizedProcessor>([this] (Scheduler::Frame const&) -> std::size_t {
		return Execute();
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

bool AuthorizedProcessor::CollectMessage(Message::Context const& context, std::string_view buffer)
{
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = Z85::Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(context, decoded);
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::CollectMessage(Message::Context const& context, std::span<std::uint8_t const> buffer)
{
    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer);
    if (!optProtocol) { return false; }

	// Handle the message based on the message protocol indicated by the message.
    switch (*optProtocol) {
        case Message::Protocol::Application: {
			auto optMessage = Message::Application::Parcel::GetBuilder()
				.SetContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			if (!optMessage) { return false; }

			return OnMessageCollected(std::move(*optMessage));
		}
		case Message::Protocol::Platform: {
			auto const optMessage = Message::Platform::Parcel::GetBuilder()
				.SetContext(context)
				.FromDecodedPack(buffer)
				.ValidatedBuild();

			if (!optMessage) { return false; }

			return OnMessageCollected(*optMessage);
		} 
        case Message::Protocol::Invalid:
		default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t AuthorizedProcessor::MessageCount() const
{
	std::shared_lock lock(m_mutex);
	return m_incoming.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t AuthorizedProcessor::Execute()
{
	if (auto const optMessage = FetchMessage(); optMessage) {
		Peer::Action::Next next{ optMessage->GetContext().GetProxy(), *optMessage, m_wpServiceProvider};
		if (!next.GetProxy().expired()) {
			// TODO: What should happen when we fail to route or handle the message. 
			[[maybe_unused]] bool const success = m_spRouter->Route(*optMessage, next);
			return 1; // Provide the number of tasks executed to the scheduler. 
		}
	}
	return 0; // Indicate that we were enable to execute a task this cycle. 
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Message::Application::Parcel> AuthorizedProcessor::FetchMessage()
{
    assert(Assertions::Threading::IsCoreThread());
	std::scoped_lock lock(m_mutex);
	if (m_incoming.empty()) { return {}; }
	auto const message = std::move(m_incoming.front());
	m_incoming.pop();
	return message;
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::OnMessageCollected(Message::Application::Parcel&& message)
{
	using namespace Message;

	// Currently, messages not destined for this node are note accepted. 
	auto const& optDestination = message.GetDestination();
    if (optDestination && *optDestination != *m_spNodeIdentifier) { return false; }

	// If the collected message is a response. then forward the processing to the tracking service. 
	auto const optAwaitable = message.GetExtension<Extension::Awaitable>(); 
	if (optAwaitable && optAwaitable->get().GetBinding() == Extension::Awaitable::Binding::Response) {
		return m_spTrackingService->Process(std::move(message));
	} else {
		std::scoped_lock lock(m_mutex);
		m_incoming.emplace(std::move(message));
		m_spDelegate->OnTaskAvailable();
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool AuthorizedProcessor::OnMessageCollected(Message::Platform::Parcel const& message)
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

	auto const spProxy = message.GetContext().GetProxy().lock();
	if (!spProxy) { return false; }

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
	return spProxy->ScheduleSend(message.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

template<>
std::optional<Message::Application::Parcel> AuthorizedProcessor::GetNextMessage<InvokeContext::Test>()
{
    return FetchMessage();
}

//----------------------------------------------------------------------------------------------------------------------
