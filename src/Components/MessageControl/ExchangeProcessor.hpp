//----------------------------------------------------------------------------------------------------------------------
// File: ExchangeProcessor.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "BryptMessage/MessageTypes.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/ExchangeObserver.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

class NetworkMessage;
class IConnectProtocol;
class ISecurityStrategy;

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
class ExchangeProcessor : public IMessageSink
{
public:
    using PreparationResult = std::pair<bool, std::string>;

    ExchangeProcessor(
        Node::SharedIdentifier const& spIdentifier,
        std::unique_ptr<ISecurityStrategy>&& upStrategy,
        std::shared_ptr<IConnectProtocol> const& spConnector,
        IExchangeObserver* const pExchangeObserver);
        
    ~ExchangeProcessor() = default;

    // IMessageSink {
    [[nodiscard]] virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpProxy, MessageContext const& context, std::string_view buffer) override;
        
    [[nodiscard]] virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpProxy,
        MessageContext const& context,
        std::span<std::uint8_t const> buffer) override;
    // }IMessageSink

    PreparationResult Prepare();
    
private:
    enum class ProcessStage : std::uint8_t { Invalid, Synchronization };

    constexpr static TimeUtils::Timestamp ExpirationPeriod = std::chrono::milliseconds(1500);

    [[nodiscard]] bool HandleMessage(std::shared_ptr<Peer::Proxy> const& spProxy, NetworkMessage const& message);
    [[nodiscard]] bool HandleSynchronizationMessage(
        std::shared_ptr<Peer::Proxy> const& spProxy, NetworkMessage const& message);

    ProcessStage m_stage;
    TimeUtils::Timepoint const m_expiration;

    Node::SharedIdentifier const m_spIdentifier;
    std::unique_ptr<ISecurityStrategy> m_upStrategy;
    std::shared_ptr<IConnectProtocol> m_spConnector;
    IExchangeObserver* const m_pExchangeObserver;

};

//----------------------------------------------------------------------------------------------------------------------
