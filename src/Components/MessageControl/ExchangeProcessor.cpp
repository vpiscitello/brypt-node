//----------------------------------------------------------------------------------------------------------------------
// File: ExchangeProcessor.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "ExchangeProcessor.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/PlatformMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/SecurityStrategy.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

ExchangeProcessor::ExchangeProcessor(
    IExchangeObserver* const pExchangeObserver,
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider,
    std::unique_ptr<ISecurityStrategy>&& upStrategy)
    : m_stage(ProcessStage::Initialization)
    , m_expiration(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
    , m_spNodeIdentifier()
    , m_spConnector(spServiceProvider->Fetch<IConnectProtocol>())
    , m_pExchangeObserver(pExchangeObserver)
    , m_upStrategy(std::move(upStrategy))
{
	if (auto const spNodeState = spServiceProvider->Fetch<NodeState>().lock(); spNodeState) {
		m_spNodeIdentifier = spNodeState->GetNodeIdentifier();
	}
	assert(m_spNodeIdentifier);
    assert(m_upStrategy);
}

//----------------------------------------------------------------------------------------------------------------------

ExchangeProcessor::ProcessStage const& ExchangeProcessor::GetProcessStage() const { return m_stage; }

//----------------------------------------------------------------------------------------------------------------------

ExchangeProcessor::PreparationResult ExchangeProcessor::Prepare()
{
    if (m_stage != ProcessStage::Initialization) { return { false, "" }; }

    auto [status, buffer] = m_upStrategy->PrepareSynchronization();

    if (status == Security::SynchronizationStatus::Error) {
        m_stage = ProcessStage::Failure;
        if (m_pExchangeObserver)  [[likely]] { m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Failed); }
        return { false, "" };
    }

    m_stage = ProcessStage::Synchronization; 

    if (buffer.empty()) { return { true, "" }; }

    auto const optRequest = Message::Platform::Parcel::GetBuilder()
        .SetSource(*m_spNodeIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(std::move(buffer))
        .ValidatedBuild();
    assert(optRequest);

    return { true, optRequest->GetPack() };
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpProxy, Message::Context const& context, std::string_view buffer)
{
    // If the exchange has been invalidated do not process the message.
    if (m_stage != ProcessStage::Synchronization) { return false; }
    
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = Z85::Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpProxy, context, decoded);
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpProxy, Message::Context const& context, std::span<std::uint8_t const> buffer)
{
    // If the exchange has been invalidated do not process the message.
    if (m_stage != ProcessStage::Synchronization) { return false; }

    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer);
    if (!optProtocol) { return false; }

    // The exchange handler may only accept handshake messages.
    switch (*optProtocol) {
        // Only process handshake messages through the exchange processor.
        case Message::Protocol::Platform: break;
        // Any other messages are should be dropped from processing.
        case Message::Protocol::Application:
        case Message::Protocol::Invalid: { m_stage = ProcessStage::Failure; } return false;
    }

    // Attempt to unpack the buffer into the handshake message.
    auto const optMessage = Message::Platform::Parcel::GetBuilder()
        .SetContext(context)
        .FromDecodedPack(buffer)
        .ValidatedBuild();

    // If the message could not be unpacked, the message cannot be handled any further. 
    if (!optMessage || optMessage->GetType() != Message::Platform::ParcelType::Handshake) { return false; }

    // The message may only be handled if the associated peer can be acquired. 
    if (auto const spProxy = wpProxy.lock(); spProxy)  [[likely]] { return OnMessageCollected(spProxy, *optMessage); }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::OnMessageCollected(
    std::shared_ptr<Peer::Proxy> const& spProxy, Message::Platform::Parcel const& message)
{
    switch (m_stage) {
        case ProcessStage::Synchronization: {
            // If the handler succeeded, break out of the switch statement. Otherwise, use the error fallthrough. 
            if (OnSynchronizationMessageCollected(spProxy, message)) { break; }
            m_stage = ProcessStage::Failure;
        } [[fallthrough]];
        default: {
            if (m_pExchangeObserver) [[likely]] { m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Failed); }
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::OnSynchronizationMessageCollected(
    std::shared_ptr<Peer::Proxy> const& spProxy, Message::Platform::Parcel const& message)
{
    assert(m_spNodeIdentifier && m_upStrategy);

    // Get the destination from the message. If the message does not have an attached identifier or the type is not
    // a node destination return an error. 
    if (message.GetDestinationType() != Message::Destination::Node) { return false; }

    // If the message has a destination, but it does not match this node's identifier then a exchange error has 
    // been encountered. It is valid if there is no destination, if the peer does not yet know our identifier. 
    auto const optDestination = message.GetDestination();
    if (optDestination && *optDestination != *m_spNodeIdentifier) { return false; }

    // Provide the attached SecurityStrategy the synchronization message.  If for some reason, the message could not
    // be handled return an error. 
    auto const [status, buffer] = m_upStrategy->Synchronize(message.GetPayload().GetReadableView());
    if (status == Security::SynchronizationStatus::Error) { return false; }

    Message::Context const& context = message.GetContext();

    // If synchronization indicated an additional message needs to be transmitted, build  the response and send it. 
    if (!buffer.empty()) {
        // Build a response to the message from the synchronization result of the strategy. 
        auto const optResponse = Message::Platform::Parcel::GetBuilder()
            .SetContext(context)
            .SetSource(*m_spNodeIdentifier)
            .SetDestination(message.GetSource())
            .MakeHandshakeMessage()
            .SetPayload(buffer)
            .ValidatedBuild();
        assert(optResponse);

        // Send the exchange message to the peer. 
        if (!spProxy->ScheduleSend(context.GetEndpointIdentifier(), optResponse->GetPack())) { return false; }
    }

    switch (status) {
        // If the synchronization indicated it has completed, notify the observer that key sharing has completed and 
        // application messages can now be completed. 
        case Security::SynchronizationStatus::Ready: {
            auto const role = m_upStrategy->GetRoleType();

            // If there is an exchange observer, provide it the prepared security strategy. 
            if (m_pExchangeObserver) [[likely]] { m_pExchangeObserver->OnFulfilledStrategy(std::move(m_upStrategy)); }

            // If there is a provided connection protocol, provide it with the proxy to send the final request. 
            if (role == Security::Role::Initiator) [[likely]] {
                auto const success = m_spConnector->SendRequest(spProxy, context);
                if (!success) { return false; }
            }

            // If there is an exchange observer, notify the observe that the exchange has successfully completed and
            // provide it the prepared security strategy. 
            if (m_pExchangeObserver) [[likely]] { m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Success); }
        } break;
        // There is no additional handling needed while the exchange is processing. 
        case Security::SynchronizationStatus::Processing: break;
        // It is an error to get this far without an early return.
        default: assert(false); return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
void ExchangeProcessor::SetStage<InvokeContext::Test>(ProcessStage stage) { m_stage = stage; }

//----------------------------------------------------------------------------------------------------------------------
