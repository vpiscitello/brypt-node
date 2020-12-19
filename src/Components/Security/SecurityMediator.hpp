//------------------------------------------------------------------------------------------------
// File: SecurityMediator.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "SecurityDefinitions.hpp"
#include "SecurityState.hpp"
#include "../MessageControl/ExchangeProcessor.hpp"
#include "../../BryptIdentifier/IdentifierTypes.hpp"
#include "../../Interfaces/ExchangeObserver.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CExchangeProcessor;
class IConnectProtocol;
class ISecurityStrategy;

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class CSecurityMediator : public IExchangeObserver
{
public:
    CSecurityMediator(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        Security::Context context,
        std::weak_ptr<IMessageSink> const& wpAuthorizedSink);

    CSecurityMediator(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        std::unique_ptr<ISecurityStrategy>&& upStrategy);

    CSecurityMediator(CSecurityMediator&& other) = delete;
    CSecurityMediator(CSecurityMediator const& other) = delete;
    CSecurityMediator& operator=(CSecurityMediator const& other) = delete;

    ~CSecurityMediator();

    // IExchangeObserver {
    virtual void OnExchangeClose(ExchangeStatus status) override;
    virtual void OnFulfilledStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy) override;
    // } IExchangeObserver

    [[nodiscard]] Security::State GetSecurityState() const;

    void BindPeer(std::shared_ptr<CBryptPeer> const& spBryptPeer);
    void BindSecurityContext(CMessageContext& context) const;

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
    std::shared_ptr<CBryptPeer> m_spBryptPeer;
    std::unique_ptr<ISecurityStrategy> m_upStrategy;

    std::unique_ptr<CExchangeProcessor> m_upExchangeProcessor;
    std::weak_ptr<IMessageSink> m_wpAuthorizedSink;

    std::shared_ptr<IConnectProtocol> m_spConnectProtocol;
};

//------------------------------------------------------------------------------------------------
