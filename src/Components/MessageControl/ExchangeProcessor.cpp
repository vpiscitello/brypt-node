//----------------------------------------------------------------------------------------------------------------------
// File: ExchangeProcessor.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "ExchangeProcessor.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/NetworkMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
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

//----------------------------------------------------------------------------------------------------------------------
// Description: 
//----------------------------------------------------------------------------------------------------------------------
ExchangeProcessor::ExchangeProcessor(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    std::shared_ptr<IConnectProtocol> const& spConnectProtocol,
    IExchangeObserver* const pExchangeObserver,
    std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy)
    : m_stage(ProcessStage::Synchronization)
    , m_expiration(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
    , m_spBryptIdentifier(spBryptIdentifier)
    , m_spConnectProtocol(spConnectProtocol)
    , m_pExchangeObserver(pExchangeObserver)
    , m_upSecurityStrategy(std::move(upSecurityStrategy))
{
    assert(m_upSecurityStrategy);
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::CollectMessage(
	std::weak_ptr<BryptPeer> const& wpBryptPeer,
    MessageContext const& context,
	std::string_view buffer)
{
    // If the exchange has been invalidated do not process the message.
    if (m_stage == ProcessStage::Invalid) { return false; }
    
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = Z85::Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpBryptPeer, context, decoded);
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::CollectMessage(
	std::weak_ptr<BryptPeer> const& wpBryptPeer,
    MessageContext const& context,
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
    auto const optMessage = NetworkMessage::Builder()
        .SetMessageContext(context)
        .FromDecodedPack(buffer)
        .ValidatedBuild();

    // If the message could not be unpacked, the message cannot be handled any further. 
    if (!optMessage || optMessage->GetMessageType() != Message::Network::Type::Handshake) {
        return false;
    }

    // The message may only be handled if the associated peer can be acquired. 
    if (auto const spBryptPeer = wpBryptPeer.lock(); spBryptPeer)  [[likely]] {
	    return HandleMessage(spBryptPeer, *optMessage);
    }

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

ExchangeProcessor::PreparationResult ExchangeProcessor::Prepare()
{
    auto const [status, buffer] = m_upSecurityStrategy->PrepareSynchronization();

    if (status == Security::SynchronizationStatus::Error) {
        m_stage = ProcessStage::Invalid;
        if (m_pExchangeObserver)  [[likely]] {
            m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Failed);
        }
        return { false, "" };
    }

    if (buffer.size() != 0) {
        auto const optRequest = NetworkMessage::Builder()
            .SetSource(*m_spBryptIdentifier)
            .MakeHandshakeMessage()
            .SetPayload(buffer)
            .ValidatedBuild();
        assert(optRequest);
        return { true, optRequest->GetPack() };
    } else {
        return { true, "" };
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::HandleMessage(
    std::shared_ptr<BryptPeer> const& spBryptPeer, NetworkMessage const& message)
{
    switch (m_stage) {
        case ProcessStage::Synchronization: {
            // If the message handling succeeded, break out of the switch statement. Otherwise,
            // fallthrough to the error handler. 
            if (HandleSynchronizationMessage(spBryptPeer, message)) { break; }
        } [[fallthrough]];
        case ProcessStage::Invalid:
        default: {
            if (m_pExchangeObserver) [[likely]] {
                m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Failed);
            }
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool ExchangeProcessor::HandleSynchronizationMessage(
    std::shared_ptr<BryptPeer> const& spBryptPeer,
    NetworkMessage const& message)
{
    assert(m_upSecurityStrategy);
    assert(m_spBryptIdentifier);

    // Provide the attached SecurityStrategy the synchronization message. 
    // If for some reason, the message could not be handled return an error. 
    auto const [status, buffer] = m_upSecurityStrategy->Synchronize(message.GetPayload());
    if (status == Security::SynchronizationStatus::Error) { return false; }

    // Get the destination from the message. If the message does not have an attached identifier
    // or the type is not a node destination return an error. 
    if (message.GetDestinationType() != Message::Destination::Node) { return false; }

    MessageContext const& context = message.GetContext();

    // If synchronization indicated an additional message needs to be transmitted, build 
    // the response and send it through the peer. 
    if (buffer.size() != 0) {
        // Build a response to the message from the synchronization result of the strategy. 
        auto const optResponse = NetworkMessage::Builder()
            .SetMessageContext(context)
            .SetSource(*m_spBryptIdentifier)
            .SetDestination(message.GetSourceIdentifier())
            .MakeHandshakeMessage()
            .SetPayload(buffer)
            .ValidatedBuild();
        assert(optResponse);

        // Send the exchange message to the peer. 
        if (!spBryptPeer->ScheduleSend(context.GetEndpointIdentifier(), optResponse->GetPack())) {
            if (m_pExchangeObserver) [[likely]] {
                m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Failed);
            }
        }
    }

    switch (status) {
        // If the synchronization indicated it has completed, notify the observer that 
        // key sharing has completed and application messages can now be completed. 
        case Security::SynchronizationStatus::Ready: {
            // If there is an excnage observer, provide it the prepared security strategy. 
            if (m_pExchangeObserver) [[likely]] {
                m_pExchangeObserver->OnFulfilledStrategy(std::move(m_upSecurityStrategy));
            }

            // If we do not interface defining the application connection protocol or if
            // that protocol fails, return an error
            if (m_spConnectProtocol) [[likely]] {
                auto const success = m_spConnectProtocol->SendRequest(
                    m_spBryptIdentifier, spBryptPeer, context);
                if (!success) { return false; }
            }

            // If there is an excnage observer, notify the observe that the exchange has 
            // successfully completed and provide it the prepared security strategy. 
            if (m_pExchangeObserver) [[likely]] {
                m_pExchangeObserver->OnExchangeClose(ExchangeStatus::Success);
            }
        } break;
        // There is no additional handling needed while the exchange is processing. 
        case Security::SynchronizationStatus::Processing: break;
        // It is an error to get this far without an early return.
        default: assert(false); return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
