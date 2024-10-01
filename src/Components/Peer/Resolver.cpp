//----------------------------------------------------------------------------------------------------------------------
// File: Resolver.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Resolver.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Message/MessageContext.hpp"
#include "Components/Processor/ExchangeProcessor.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Interfaces/ConnectProtocol.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: 
//----------------------------------------------------------------------------------------------------------------------
Peer::Resolver::Resolver()
    : m_mutex()
    , m_upExchange()
    , m_onStrategyFulfilled()
    , m_onExchangeCompleted()
    , m_completed(false)
{
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Resolver& Peer::Resolver::operator=(Resolver&& other)
{
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
    if (!m_completed && m_onExchangeCompleted) {
        m_onExchangeCompleted(ExchangeStatus::Failed);
    }
    m_upExchange.reset();
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Resolver::OnExchangeClose(ExchangeStatus status)
{
    std::scoped_lock lock{ m_mutex };
    m_completed = true;
    // If the completion handlers have been bound, the proxy should destroy this resolver instance in this call. 
    // It is no longer safe to use our resources after this call completes. 
    if (m_onExchangeCompleted) [[likely]] {
        m_onExchangeCompleted(status);
    }   
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Resolver::OnFulfilledStrategy(std::unique_ptr<Security::CipherPackage>&& upCipherPackage)
{
    std::scoped_lock lock{ m_mutex };
    assert(m_onStrategyFulfilled && upCipherPackage);
    m_onStrategyFulfilled(std::move(upCipherPackage));
}

//----------------------------------------------------------------------------------------------------------------------

IMessageSink* Peer::Resolver::GetExchangeSink() const
{
    return m_upExchange.get();
}

//----------------------------------------------------------------------------------------------------------------------

void Peer::Resolver::BindCompletionHandlers(OnStrategyFulfilled const& onFulfilled, OnExchangeCompleted const& onCompleted)
{
    std::scoped_lock lock{ m_mutex };
    assert(m_upExchange && (onFulfilled && onCompleted));
    m_onStrategyFulfilled = onFulfilled;
    m_onExchangeCompleted = onCompleted;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> Peer::Resolver::SetupExchangeInitiator(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    // This method should only be called for the initial exchange, another method should be used to resynchronize.
    std::scoped_lock lock{ m_mutex };
    if (m_upExchange) [[unlikely]] { return {}; }

    // The processor will process the handshake message and use the strategy to negotiate keys to initialize state.
    using Result = std::optional<std::string>;
    m_upExchange = std::make_unique<ExchangeProcessor>(Security::ExchangeRole::Initiator, spServiceProvider, this);
    auto [success, request] = m_upExchange->Prepare(); // Provide the caller the request to be sent to the peer.
    return (success) ? Result(std::move(request)) : std::nullopt;  
}

//----------------------------------------------------------------------------------------------------------------------

bool Peer::Resolver::SetupExchangeAcceptor(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    // This method should only be called for the initial exchange, another method should be used to resynchronize.
    std::scoped_lock lock{ m_mutex };
    if (m_upExchange) [[unlikely]] { return false; }

    // The processor will process the handshake message and use the strategy to negotiate keys to initialize state.
    m_upExchange = std::make_unique<ExchangeProcessor>(Security::ExchangeRole::Acceptor, spServiceProvider, this);
    auto const [success, request] = m_upExchange->Prepare();
    assert(request.empty());
    return success; 
}

//----------------------------------------------------------------------------------------------------------------------

template<>
bool Peer::Resolver::SetupCustomExchange<InvokeContext::Test>(
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider,
    std::unique_ptr<ISynchronizer>&& upSynchronizer)
{
    std::scoped_lock lock{ m_mutex };
    if (m_upExchange) [[unlikely]] { return false; }

    m_upExchange = std::make_unique<ExchangeProcessor>(spServiceProvider, std::move(upSynchronizer), this);
    m_upExchange->SetStage<InvokeContext::Test>(ExchangeProcessor::ProcessStage::Synchronization);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
