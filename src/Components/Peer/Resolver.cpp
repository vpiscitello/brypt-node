//----------------------------------------------------------------------------------------------------------------------
// File: Resolver.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Resolver.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptMessage/MessageContext.hpp"
#include "Components/MessageControl/ExchangeProcessor.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: 
//----------------------------------------------------------------------------------------------------------------------
Peer::Resolver::Resolver(Node::SharedIdentifier const& spSource, Security::Context context)
    : m_mutex()
    , m_context(context)
    , m_spSource(spSource)
    , m_upExchange()
    , m_onStrategyFulfilled()
    , m_onExchangeCompleted()
    , m_completed(false)
{
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Resolver& Peer::Resolver::operator=(Resolver&& other)
{
    m_context = other.m_context;
    m_spSource = std::move(other.m_spSource);
    m_upExchange = std::move(other.m_upExchange);
    m_onStrategyFulfilled = std::move(other.m_onStrategyFulfilled);
    m_onExchangeCompleted = std::move(other.m_onExchangeCompleted);
    m_completed = other.m_completed;
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Resolver::~Resolver()
{
    // If the exchange is still ongoing and there is a proxy waiting for notification, notify it that it has failed. 
    if (!m_completed && m_onExchangeCompleted) { m_onExchangeCompleted(ExchangeStatus::Failed); }
    m_upExchange.reset();
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Resolver::OnExchangeClose(ExchangeStatus status)
{
    std::scoped_lock lock(m_mutex);
    m_completed = true;
    // If the completion handlers have been bound, the proxy should destroy this resolver instance in this call. 
    // It is no longer safe to use our resources after this call completes. 
    if (m_onExchangeCompleted) [[likely]] { m_onExchangeCompleted(status); }   
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Resolver::OnFulfilledStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy)
{
    std::scoped_lock lock(m_mutex);
    assert(m_onStrategyFulfilled && upStrategy);
    m_onStrategyFulfilled(std::move(upStrategy));
}

//----------------------------------------------------------------------------------------------------------------------

IMessageSink* Peer::Resolver::GetExchangeSink() const
{
    return m_upExchange.get();
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Resolver::BindCompletionHandlers(
    OnStrategyFulfilled const& onFulfilled, OnExchangeCompleted const& onCompleted)
{
    std::scoped_lock lock(m_mutex);
    assert(m_upExchange && (onFulfilled && onCompleted));
    m_onStrategyFulfilled = onFulfilled;
    m_onExchangeCompleted = onCompleted;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> Peer::Resolver::SetupExchangeInitiator(
    Security::Strategy strategy, std::shared_ptr<IConnectProtocol> const& spProtocol)
{
    // This method should only be called for the initial exchange, another method should be used to resynchronize.
    std::scoped_lock lock(m_mutex);
    if (m_upExchange) [[unlikely]] { return {}; }
    auto upStrategy = Security::CreateStrategy(strategy, Security::Role::Initiator, m_context);

    // The processor will process the handshake meessage and use the strategy to negiotiate keys to initialize state.
    using Result = std::optional<std::string>;
    if (!SetupExchangeProcessor(std::move(upStrategy), spProtocol)) { return {}; }
    auto [success, request] = m_upExchange->Prepare(); // Provide the caller the request to be sent to the peer.
    return (success) ? Result(std::move(request)) : std::nullopt;  
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Resolver::SetupExchangeAcceptor(Security::Strategy strategy)
{
    // This method should only be called for the initial exchange, another method should be used to resynchronize.
    std::scoped_lock lock(m_mutex);
    if (m_upExchange) [[unlikely]] { return false; }
    auto upStrategy = Security::CreateStrategy(strategy, Security::Role::Acceptor, m_context);

    // The processor will process the handshake meessage and use the strategy to negiotiate keys to initialize state.
    if (!SetupExchangeProcessor(std::move(upStrategy), nullptr)) { return false; }
    auto const [success, request] = m_upExchange->Prepare();
    return success; 
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Resolver::SetupExchangeProcessor(
    std::unique_ptr<ISecurityStrategy>&& upStrategy, std::shared_ptr<IConnectProtocol> const& spProtocol)
{
    assert(!m_completed);
    if (m_upExchange || !upStrategy) [[unlikely]] { return false; }
    m_upExchange = std::make_unique<ExchangeProcessor>(m_spSource, std::move(upStrategy), spProtocol, this);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
bool Peer::Resolver::SetupTestProcessor<InvokeContext::Test>(std::unique_ptr<ISecurityStrategy>&& upStrategy)
{
    return SetupExchangeProcessor(std::move(upStrategy), nullptr);
}

//----------------------------------------------------------------------------------------------------------------------
