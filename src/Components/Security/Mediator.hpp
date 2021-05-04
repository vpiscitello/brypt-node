//----------------------------------------------------------------------------------------------------------------------
// File: Mediator.hpp
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

namespace Peer { class Proxy; }

class ExchangeProcessor;
class IConnectProtocol;
class ISecurityStrategy;

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

class Mediator;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::Mediator final : public IExchangeObserver
{
public:
    Mediator(
        Node::SharedIdentifier const& spNodeIdentifier,
        Security::Context context,
        std::weak_ptr<IMessageSink> const& wpAuthorizedSink);

    Mediator( Node::SharedIdentifier const& spNodeIdentifier, std::unique_ptr<ISecurityStrategy>&& upStrategy);

    Mediator(Mediator&& other) = delete;
    Mediator(Mediator const& other) = delete;
    Mediator& operator=(Mediator const& other) = delete;

    ~Mediator();

    // IExchangeObserver {
    virtual void OnExchangeClose(ExchangeStatus status) override;
    virtual void OnFulfilledStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy) override;
    // } IExchangeObserver

    [[nodiscard]] Security::State GetSecurityState() const;

    void BindPeer(std::shared_ptr<Peer::Proxy> const& spPeerProxy);
    void BindSecurityContext(MessageContext& context) const;

    [[nodiscard]] std::optional<std::string> SetupExchangeInitiator(
        Security::Strategy strategy, std::shared_ptr<IConnectProtocol> const& spConnectProtocol);
    [[nodiscard]] bool SetupExchangeAcceptor(Security::Strategy strategy);
    [[nodiscard]] bool SetupExchangeProcessor(
        std::unique_ptr<ISecurityStrategy>&& upStrategy, std::shared_ptr<IConnectProtocol> const& spConnectProtocol);

private:
    mutable std::shared_mutex m_mutex;

    Security::Context m_context;
    Security::State m_state;

    Node::SharedIdentifier m_spNodeIdentifier;
    std::shared_ptr<Peer::Proxy> m_spPeerProxy;
    std::unique_ptr<ISecurityStrategy> m_upStrategy;

    std::unique_ptr<ExchangeProcessor> m_upExchangeProcessor;
    std::weak_ptr<IMessageSink> m_wpAuthorizedSink;

    std::shared_ptr<IConnectProtocol> m_spConnectProtocol;
};

//----------------------------------------------------------------------------------------------------------------------
