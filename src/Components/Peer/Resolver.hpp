//----------------------------------------------------------------------------------------------------------------------
// File: Resolver.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SecurityState.hpp"
#include "Components/MessageControl/ExchangeProcessor.hpp"
#include "Interfaces/ExchangeObserver.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <functional>
#include <shared_mutex>
//----------------------------------------------------------------------------------------------------------------------

class ExchangeProcessor;
class IConnectProtocol;
class ISecurityStrategy;

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Proxy;
class Resolver;

//----------------------------------------------------------------------------------------------------------------------
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Resolver final : public IExchangeObserver
{
public:
    using OnExchangeCompleted = std::function<void(ExchangeStatus)>;
    using OnStrategyFulfilled = std::function<void(std::unique_ptr<ISecurityStrategy>&& upStrategy)>;

    Resolver(Node::SharedIdentifier const& spIdentifier, Security::Context context);
    ~Resolver();

    Resolver(Resolver&& other) = delete;
    Resolver(Resolver const& other) = delete;
    Resolver& operator=(Resolver const& other) = delete;
    Resolver& operator=(Resolver&& other);

    // IExchangeObserver {
    virtual void OnExchangeClose(ExchangeStatus status) override;
    virtual void OnFulfilledStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy) override;
    // } IExchangeObserver

    [[nodiscard]] IMessageSink* GetExchangeSink() const;
    void BindCompletionHandlers(OnStrategyFulfilled const& onFulfilled, OnExchangeCompleted const& onCompleted);

    [[nodiscard]] std::optional<std::string> SetupExchangeInitiator(
        Security::Strategy strategy, std::shared_ptr<IConnectProtocol> const& spProtocol);
    [[nodiscard]] bool SetupExchangeAcceptor(Security::Strategy strategy);

    // Testing Methods  {
    UT_SupportMethod([[nodiscard]] bool SetupTestProcessor(std::unique_ptr<ISecurityStrategy>&& upStrategy));
    // } Testing Methods 

private:
    [[nodiscard]] bool SetupExchangeProcessor(
        std::unique_ptr<ISecurityStrategy>&& upStrategy, std::shared_ptr<IConnectProtocol> const& spProtocol);
        
    mutable std::shared_mutex m_mutex;

    Security::Context m_context;
    Node::SharedIdentifier m_spIdentifier;
    std::unique_ptr<ExchangeProcessor> m_upExchange;
    OnStrategyFulfilled m_onStrategyFulfilled;
    OnExchangeCompleted m_onExchangeCompleted;
    bool m_completed;
};

//----------------------------------------------------------------------------------------------------------------------
