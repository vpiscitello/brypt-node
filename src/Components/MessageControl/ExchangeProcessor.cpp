//----------------------------------------------------------------------------------------------------------------------
// File: ExchangeProcessor.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "ExchangeProcessor.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/NetworkMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
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
    Node::SharedIdentifier const& spSource,
    std::unique_ptr<ISecurityStrategy>&& upStrategy,
    std::shared_ptr<IConnectProtocol> const& spConnector,
    IExchangeObserver* const pExchangeObserver)
    : m_stage(ProcessStage::Synchronization)
    , m_expiration(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
    , m_spSource(spSource)
    , m_upStrategy(std::move(upStrategy))
    , m_spConnector(spConnector)
    , m_pExchangeObserver(pExchangeObserver)
{
    assert(m_upStrategy);
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpProxy, Message::Context const& context, std::string_view buffer)
{
    // If the exchange has been invalidated do not process the message.
    if (m_stage == ProcessStage::Invalid) { return false; }
    
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = Z85::Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpProxy, context, decoded);
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::CollectMessage(
	std::weak_ptr<Peer::Proxy> const& wpProxy,
    Message::Context const& context,
	std::span<std::uint8_t const> buffer)
{
    // If the exchange has been invalidated do not process the message.
    if (m_stage == ProcessStage::Invalid) { return false; }

    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer);
    if (!optProtocol) { return false; }

    // The exchange handler may only accept handshake messages.
    switch (*optProtocol) {
        // Only process handshake messages through the exchange processor.
        case Message::Protocol::Network: break;
        // Any other messages are should be dropped from processing.
        case Message::Protocol::Application:
        case Message::Protocol::Invalid: { m_stage = ProcessStage::Invalid; } return false;
    }

    // Attempt to unpack the buffer into the handshake message.
    auto const optMessage = Message::Network::Parcel::GetBuilder()
        .SetContext(context)
        .FromDecodedPack(buffer)
        .ValidatedBuild();

    // If the message could not be unpacked, the message cannot be handled any further. 
    if (!optMessage || optMessage->GetType() != Message::Network::Type::Handshake) { return false; }

    // The message may only be handled if the associated peer can be acquired. 
    if (auto const spProxy = wpProxy.lock(); spProxy)  [[likely]] { return HandleMessage(spProxy, *optMessage); }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

ExchangeProcessor::PreparationResult ExchangeProcessor::Prepare()
{
    auto const [status, buffer] = m_upStrategy->PrepareSynchronization();

    if (status == Security::SynchronizationStatus::Error) {
        m_stage = ProcessStage::Invalid;
        if (m_pExchangeObserver)  [[likely]] { m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Failed); }
        return { false, "" };
    }

    if (buffer.size() == 0) { return { true, "" }; }

    auto const optRequest = Message::Network::Parcel::GetBuilder()
        .SetSource(*m_spSource)
        .MakeHandshakeMessage()
        .SetPayload(std::move(buffer))
        .ValidatedBuild();
    assert(optRequest);
    return { true, optRequest->GetPack() };
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::HandleMessage(
    std::shared_ptr<Peer::Proxy> const& spProxy, Message::Network::Parcel const& message)
{
    switch (m_stage) {
        case ProcessStage::Synchronization: {
            // If the handler succeeded, break out of the switch statement. Otherwise, use the error fallthrough. 
            if (HandleSynchronizationMessage(spProxy, message)) { break; }
        } [[fallthrough]];
        case ProcessStage::Invalid:
        default: {
            if (m_pExchangeObserver) [[likely]] {  m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Failed); }
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::HandleSynchronizationMessage(
    std::shared_ptr<Peer::Proxy> const& spProxy,
    Message::Network::Parcel const& message)
{
    assert(m_spSource && m_upStrategy);

    // Provide the attached SecurityStrategy the synchronization message. 
    // If for some reason, the message could not be handled return an error. 
    auto const [status, buffer] = m_upStrategy->Synchronize(message.GetPayload());
    if (status == Security::SynchronizationStatus::Error) { return false; }

    // Get the destination from the message. If the message does not have an attached identifier
    // or the type is not a node destination return an error. 
    if (message.GetDestinationType() != Message::Destination::Node) { return false; }

    Message::Context const& context = message.GetContext();

    // If synchronization indicated an additional message needs to be transmitted, build 
    // the response and send it through the peer. 
    if (buffer.size() != 0) {
        // Build a response to the message from the synchronization result of the strategy. 
        auto const optResponse = Message::Network::Parcel::GetBuilder()
            .SetContext(context)
            .SetSource(*m_spSource)
            .SetDestination(message.GetSourceIdentifier())
            .MakeHandshakeMessage()
            .SetPayload(buffer)
            .ValidatedBuild();
        assert(optResponse);

        // Send the exchange message to the peer. 
        if (!spProxy->ScheduleSend(context.GetEndpointIdentifier(), optResponse->GetPack())) {
            if (m_pExchangeObserver) [[likely]] { m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Failed); }
        }
    }

    switch (status) {
        // If the synchronization indicated it has completed, notify the observer that 
        // key sharing has completed and application messages can now be completed. 
        case Security::SynchronizationStatus::Ready: {
            // If there is an excnage observer, provide it the prepared security strategy. 
            if (m_pExchangeObserver) [[likely]] { m_pExchangeObserver->OnFulfilledStrategy(std::move(m_upStrategy)); }

            // If there is a provided connection protocol, provide it with the proxy to send the final request. 
            if (m_spConnector) [[likely]] {
                auto const success = m_spConnector->SendRequest(m_spSource, spProxy, context);
                if (!success) { return false; }
            }

            // If there is an excnage observer, notify the observe that the exchange has 
            // successfully completed and provide it the prepared security strategy. 
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
