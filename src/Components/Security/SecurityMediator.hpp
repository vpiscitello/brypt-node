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
        std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy,
        std::weak_ptr<IMessageSink> const& wpAuthorizedSink);

    CSecurityMediator(CSecurityMediator&& other) = delete;
    CSecurityMediator(CSecurityMediator const& other) = delete;
    CSecurityMediator& operator=(CSecurityMediator const& other) = delete;

    ~CSecurityMediator();

    // IExchangeObserver {
    virtual void HandleExchangeClose(
        ExchangeStatus status,
        std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy = nullptr) override;
    // } IExchangeObserver

    Security::State GetSecurityState() const;

    void Bind(std::shared_ptr<CBryptPeer> const& spBryptPeer);

    std::optional<std::string> SetupExchangeInitiator(
        Security::Strategy strategy,
        IConnectProtocol const* const pConnectProtocol);
    bool SetupExchangeAcceptor(Security::Strategy strategy);

private:
    Security::Context m_context;
    Security::State m_state;

    BryptIdentifier::SharedContainer m_spBryptIdentifier;
    std::shared_ptr<CBryptPeer> m_spBryptPeer;
    std::unique_ptr<ISecurityStrategy> m_upSecurityStrategy;

    std::unique_ptr<CExchangeProcessor> m_upExchangeProcessor;
    std::weak_ptr<IMessageSink> m_wpAuthorizedSink;

};

//------------------------------------------------------------------------------------------------
