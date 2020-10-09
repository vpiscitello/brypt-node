//------------------------------------------------------------------------------------------------
// File: ExchangeProcessor.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Security/SecurityState.hpp"
#include "../../BryptIdentifier/IdentifierTypes.hpp"
#include "../../BryptMessage/MessageTypes.hpp"
#include "../../Interfaces/ExchangeObserver.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <memory>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CHandshakeMessage;
class ISecurityStrategy;

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class CExchangeProcessor : public IMessageSink
{
public:
    CExchangeProcessor(
        IExchangeObserver* const pExchangeObserver,
        std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy);

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        Message::Buffer const& buffer) override;
    // }IMessageSink
    
private:
    enum class ProcessStage : std::uint8_t {
        Invalidated, Initialization, KeySharing, Verification, Finalization };

    constexpr static TimeUtils::Timestamp ExpirationPeriod = std::chrono::milliseconds(1500);

    bool HandleMessage(
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        CHandshakeMessage const& message);

    ProcessStage m_stage;
    TimeUtils::Timepoint const m_expiration;

    IExchangeObserver* const m_pExchangeObserver;
    std::unique_ptr<ISecurityStrategy>&& m_upSecurityStrategy;

};

//------------------------------------------------------------------------------------------------
