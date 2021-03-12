//----------------------------------------------------------------------------------------------------------------------
// File: SecurityMediator.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "SecurityDefinitions.hpp"
#include "SecurityState.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/MessageControl/ExchangeProcessor.hpp"
#include "Interfaces/ExchangeObserver.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <shared_mutex>
//----------------------------------------------------------------------------------------------------------------------

class BryptPeer;
class ExchangeProcessor;
class IConnectProtocol;
class ISecurityStrategy;

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
class SecurityMediator : public IExchangeObserver
{
public:
    SecurityMediator(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        Security::Context context,
        std::weak_ptr<IMessageSink> const& wpAuthorizedSink);

    SecurityMediator(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        std::unique_ptr<ISecurityStrategy>&& upStrategy);

    SecurityMediator(SecurityMediator&& other) = delete;
    SecurityMediator(SecurityMediator const& other) = delete;
    SecurityMediator& operator=(SecurityMediator const& other) = delete;

    ~SecurityMediator();

    // IExchangeObserver {
    virtual void OnExchangeClose(ExchangeStatus status) override;
    virtual void OnFulfilledStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy) override;
    // } IExchangeObserver

    [[nodiscard]] Security::State GetSecurityState() const;

    void BindPeer(std::shared_ptr<BryptPeer> const& spBryptPeer);
    void BindSecurityContext(MessageContext& context) const;

    [[nodiscard]] std::optional<std::string> SetupExchangeInitiator(
        Security::Strategy strategy,
        std::shared_ptr<IConnectProtocol> const& spConnectProtocol);
    [[nodiscard]] bool SetupExchangeAcceptor(Security::Strategy strategy);
    [[nodiscard]] bool SetupExchangeProcessor(
        std::unique_ptr<ISecurityStrategy>&& upStrategy,
        std::shared_ptr<IConnectProtocol> const& spConnectProtocol);

private:
    mutable std::shared_mutex m_mutex;

    Security::Context m_context;
    Security::State m_state;

    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    std::shared_ptr<BryptPeer> m_spBryptPeer;
    std::unique_ptr<ISecurityStrategy> m_upStrategy;

    std::unique_ptr<ExchangeProcessor> m_upExchangeProcessor;
    std::weak_ptr<IMessageSink> m_wpAuthorizedSink;

    std::shared_ptr<IConnectProtocol> m_spConnectProtocol;
};

//----------------------------------------------------------------------------------------------------------------------
