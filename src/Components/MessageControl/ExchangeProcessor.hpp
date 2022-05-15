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
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

namespace Message::Platform { class Parcel; }
namespace Node { class ServiceProvider; }
namespace Peer { class Proxy; }

class IConnectProtocol;
class ISecurityStrategy;

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
class ExchangeProcessor : public IMessageSink
{
public:
    enum class ProcessStage : std::uint32_t { Failure, Initialization, Synchronization };
    using PreparationResult = std::pair<bool, std::string>;

    ExchangeProcessor(
        IExchangeObserver* const pExchangeObserver,
        std::shared_ptr<Node::ServiceProvider> const& spServiceProvider,
        std::unique_ptr<ISecurityStrategy>&& upStrategy);
        
    ~ExchangeProcessor() = default;

    ProcessStage const& GetProcessStage() const;

    PreparationResult Prepare();

    // IMessageSink {
    [[nodiscard]] virtual bool CollectMessage(Message::Context const& context, std::string_view buffer) override;
    [[nodiscard]] virtual bool CollectMessage(
        Message::Context const& context, std::span<std::uint8_t const> buffer) override;
    // }IMessageSink
    
    UT_SupportMethod(void SetStage(ProcessStage stage));

private:

    static constexpr TimeUtils::Timestamp ExpirationPeriod = std::chrono::milliseconds(1500);

    [[nodiscard]] bool OnMessageCollected(
        std::shared_ptr<Peer::Proxy> const& spProxy, Message::Platform::Parcel const& message);

    [[nodiscard]] bool OnSynchronizationMessageCollected(
        std::shared_ptr<Peer::Proxy> const& spProxy, Message::Platform::Parcel const& message);

    ProcessStage m_stage;
    TimeUtils::Timepoint const m_expiration;

    Node::SharedIdentifier m_spNodeIdentifier;
    std::shared_ptr<IConnectProtocol> m_spConnector;
    IExchangeObserver* const m_pExchangeObserver;
    std::unique_ptr<ISecurityStrategy> m_upStrategy;
};

//----------------------------------------------------------------------------------------------------------------------
