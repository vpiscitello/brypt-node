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
        Node::SharedIdentifier const& spNodeIdentifier,
        std::shared_ptr<IConnectProtocol> const& spConnectProtocol,
        IExchangeObserver* const pExchangeObserver,
        std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy);

    // IMessageSink {
    [[nodiscard]] virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        MessageContext const& context,
        std::string_view buffer) override;
        
    [[nodiscard]] virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        MessageContext const& context,
        std::span<std::uint8_t const> buffer) override;
    // }IMessageSink

    PreparationResult Prepare();
    
private:
    enum class ProcessStage : std::uint8_t { Invalid, Synchronization };

    constexpr static TimeUtils::Timestamp ExpirationPeriod = std::chrono::milliseconds(1500);

    [[nodiscard]] bool HandleMessage(
        std::shared_ptr<Peer::Proxy> const& spPeerProxy, NetworkMessage const& message);

    [[nodiscard]] bool HandleSynchronizationMessage(
        std::shared_ptr<Peer::Proxy> const& spPeerProxy, NetworkMessage const& message);

    ProcessStage m_stage;
    TimeUtils::Timepoint const m_expiration;

    Node::SharedIdentifier const m_spNodeIdentifier;
    std::shared_ptr<IConnectProtocol> m_spConnectProtocol;
    IExchangeObserver* const m_pExchangeObserver;
    std::unique_ptr<ISecurityStrategy> m_upSecurityStrategy;

};

//----------------------------------------------------------------------------------------------------------------------
