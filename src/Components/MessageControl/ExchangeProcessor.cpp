//------------------------------------------------------------------------------------------------
// File: ExchangeProcessor.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ExchangeProcessor.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/HandshakeMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../BryptMessage/MessageUtils.hpp"
#include "../../BryptMessage/PackUtils.hpp"
#include "../../Interfaces/SecurityStrategy.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
// Description: 
//------------------------------------------------------------------------------------------------
CExchangeProcessor::CExchangeProcessor(
    IExchangeObserver* const pExchangeObserver,
    std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy)
    : m_stage(ProcessStage::Initialization)
    , m_expiration(TimeUtils::GetSystemTimepoint() + ExpirationPeriod)
    , m_pExchangeObserver(pExchangeObserver)
    , m_upSecurityStrategy(std::move(upSecurityStrategy))
{
}

//------------------------------------------------------------------------------------------------

bool CExchangeProcessor::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CMessageContext const& context,
	std::string_view buffer)
{
    // If the exchange has been invalidated do not process the message.
    if (m_stage == ProcessStage::Invalidated) {
        return false;
    }
    
    // Decode the buffer as it is expected to be encoded with Z85.
    Message::Buffer const decoded = PackUtils::Z85Decode(buffer);

    // Pass on the message collection to the decoded buffer method. 
    return CollectMessage(wpBryptPeer, context, decoded);
}

//------------------------------------------------------------------------------------------------

bool CExchangeProcessor::CollectMessage(
	std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CMessageContext const& context,
	Message::Buffer const& buffer)
{
    // If the exchange has been invalidated do not process the message.
    if (m_stage == ProcessStage::Invalidated) {
        return false;
    }

    // Peek the protocol in the packed buffer. 
    auto const optProtocol = Message::PeekProtocol(buffer.begin(), buffer.end());
    if (!optProtocol) {
        return false;
    }

    // The exchange handler may only accept handshake messages.
    switch (*optProtocol) {
        // Only process handshake messages through the exchange processor.
        case Message::Protocol::Handshake: break;
        // Any other messages are should be dropped from processing.
        case Message::Protocol::Application:
        case Message::Protocol::Invalid: {
            m_stage = ProcessStage::Invalidated;
        } return false;
    }

    // Attempt to unpack the buffer into the handshake message.
    auto const optMessage = CHandshakeMessage::Builder()
        .SetMessageContext(context)
        .FromDecodedPack(buffer)
        .ValidatedBuild();

    // If the message could not be unpacked, the message cannot be handled any further. 
    if (!optMessage) {
        return false;
    }

    // The message may only be handled if the associated peer can be acquired. 
    if (auto const spBryptPeer = wpBryptPeer.lock(); spBryptPeer) {
	    return HandleMessage(spBryptPeer, *optMessage);
    }

    return false;
}

//------------------------------------------------------------------------------------------------

bool CExchangeProcessor::HandleMessage(
    std::shared_ptr<CBryptPeer> const& spBryptPeer,
    CHandshakeMessage const& message)
{
    return true;
}

//------------------------------------------------------------------------------------------------
