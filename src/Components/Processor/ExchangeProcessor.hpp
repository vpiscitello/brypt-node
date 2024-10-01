//----------------------------------------------------------------------------------------------------------------------
// File: ExchangeProcessor.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Identifier/IdentifierTypes.hpp"
#include "Components/Message/MessageTypes.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Interfaces/ExchangeObserver.hpp"
#include "Interfaces/MessageSink.hpp"
#include "Interfaces/Synchronizer.hpp"
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

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
class ExchangeProcessor : public IMessageSink
{
public:
    enum class ProcessStage : std::uint32_t { Failure, Initialization, Synchronization };
    using PreparationResult = std::pair<bool, std::string>;

    ExchangeProcessor(
        Security::ExchangeRole role,
        std::shared_ptr<Node::ServiceProvider> const& spServiceProvider,
        IExchangeObserver* const pExchangeObserver);
    
    ExchangeProcessor(
        std::shared_ptr<Node::ServiceProvider> const& spServiceProvider,
        std::unique_ptr<ISynchronizer>&& upSynchronizer,
        IExchangeObserver* const pExchangeObserver);

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
    static constexpr TimeUtils::Timestamp ExpirationPeriod{ 1500 };

    [[nodiscard]] bool OnMessageCollected(Message::Context const& context, std::span<std::uint8_t const> buffer);
    [[nodiscard]] bool OnMessageCollected(std::shared_ptr<Peer::Proxy> const& spProxy, Message::Platform::Parcel const& message);

    ProcessStage m_stage;
    TimeUtils::Timepoint const m_expiration;

    Node::SharedIdentifier m_spNodeIdentifier;
    std::shared_ptr<IConnectProtocol> m_spConnector;
    std::unique_ptr<ISynchronizer> m_upSynchronizer;
    IExchangeObserver* const m_pExchangeObserver;
};

//----------------------------------------------------------------------------------------------------------------------
