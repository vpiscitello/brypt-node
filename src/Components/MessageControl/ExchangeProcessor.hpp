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
#include <utility>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CNetworkMessage;
class IConnectProtocol;
class ISecurityStrategy;

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class CExchangeProcessor : public IMessageSink
{
public:
    using PreparationResult = std::pair<bool, std::string>;

    CExchangeProcessor(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        IConnectProtocol const* const pConnectProtocol,
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

    PreparationResult Prepare();
    
private:
    enum class ProcessStage : std::uint8_t { Invalid, Synchronization };

    constexpr static TimeUtils::Timestamp ExpirationPeriod = std::chrono::milliseconds(1500);

    bool HandleMessage(
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        CNetworkMessage const& message);

    bool HandleSynchronizationMessage(
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        CNetworkMessage const& message);

    ProcessStage m_stage;
    TimeUtils::Timepoint const m_expiration;

    BryptIdentifier::SharedContainer const m_spBryptIdentifier;
    IConnectProtocol const* const m_pConnectProtocol;
    IExchangeObserver* const m_pExchangeObserver;
    std::unique_ptr<ISecurityStrategy> m_upSecurityStrategy;

};

//------------------------------------------------------------------------------------------------
