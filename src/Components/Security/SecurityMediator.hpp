//------------------------------------------------------------------------------------------------
// File: SecurityMediator.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "SecurityState.hpp"
#include "../MessageControl/ExchangeProcessor.hpp"
#include "../../BryptIdentifier/IdentifierTypes.hpp"
#include "../../Interfaces/ExchangeObserver.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CExchangeProcessor;
class ISecurityStrategy;

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class CSecurityMediator : public IExchangeObserver
{
public:
    CSecurityMediator(
        std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy,
        std::weak_ptr<IMessageSink> const& wpAuthenticatedSink);

    CSecurityMediator(CSecurityMediator&& other) = delete;
    CSecurityMediator(CSecurityMediator const& other) = delete;
    CSecurityMediator& operator=(CSecurityMediator const& other) = delete;

    ~CSecurityMediator();

    // IExchangeObserver {
    virtual void HandleExchangeClose(
        ExchangeStatus status,
        std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy = nullptr) override;
    // } IExchangeObserver

    SecurityState GetSecurityState() const;

    void Bind(std::shared_ptr<CBryptPeer> const& spBryptPeer);

private:
    void PrepareExchange();

    SecurityState m_state;

    std::unique_ptr<ISecurityStrategy> m_upSecurityStrategy;
    std::shared_ptr<CBryptPeer> m_spBryptPeer;

    std::unique_ptr<CExchangeProcessor> m_upExchangeProcessor;
    std::weak_ptr<IMessageSink> m_wpAuthenticatedSink;

};

//------------------------------------------------------------------------------------------------
