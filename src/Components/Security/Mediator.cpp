//----------------------------------------------------------------------------------------------------------------------
// File: Mediator.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Mediator.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "SecurityUtils.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/MessageControl/ExchangeProcessor.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: 
//----------------------------------------------------------------------------------------------------------------------
Security::Mediator::Mediator(
    Node::SharedIdentifier const& spNodeIdentifier,
    Security::Context context,
    std::weak_ptr<IMessageSink> const& wpAuthorizedSink)
    : m_mutex()
    , m_context(context)
    , m_state(Security::State::Unauthorized)
    , m_spNodeIdentifier(spNodeIdentifier)
    , m_spPeer()
    , m_upStrategy()
    , m_upExchangeProcessor()
    , m_wpAuthorizedSink(wpAuthorizedSink)
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::Mediator::Mediator(
    Node::SharedIdentifier const& spNodeIdentifier, std::unique_ptr<ISecurityStrategy>&& upStrategy)
    : m_mutex()
    , m_context(upStrategy->GetContextType())
    , m_state(Security::State::Unauthorized)
    , m_spNodeIdentifier(spNodeIdentifier)
    , m_spPeer()
    , m_upStrategy(std::move(upStrategy))
    , m_upExchangeProcessor()
    , m_wpAuthorizedSink()
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::Mediator::~Mediator()
{
    // If the unauthorized sink is still active for the peer, we must unset the peer's receiver
    // to ensure the receiver does not point to invalid memory. Note: the process of acquiring the 
    // receiver mutex in Peer::Proxy should ensure that the sink is not destructed while it is 
    // actively processing a message. 
    if (m_spPeer && m_upExchangeProcessor) { m_spPeer->SetReceiver(nullptr); }
}

//----------------------------------------------------------------------------------------------------------------------

void Security::Mediator::OnExchangeClose(ExchangeStatus status)
{
    std::scoped_lock lock(m_mutex);
    if (!m_spPeer) [[unlikely]] { return; }   
    
    switch (status) {
        // If we have been notified us of a successful exchange set the message sink for the peer 
        // to the authorized sink and mark the peer as authorized. 
        case ExchangeStatus::Success: {
            m_state = Security::State::Authorized;
            if (auto const spMessageSink = m_wpAuthorizedSink.lock(); spMessageSink) [[likely]] {
                m_spPeer->SetReceiver(spMessageSink.get());
            } else { assert(false); }
        } break;
        // If we have been notified us of a failed exchange unset the message sink for the peer 
        // and mark the peer as unauthorized. 
        case ExchangeStatus::Failed: {
            m_state = Security::State::Unauthorized;
            m_spPeer->SetReceiver(nullptr);
        } break;
        default: assert(false); break;  // What is this?
    }

    m_upExchangeProcessor.reset(); // Delete the exchange processor. 
}

//----------------------------------------------------------------------------------------------------------------------

void Security::Mediator::OnFulfilledStrategy(std::unique_ptr<ISecurityStrategy>&& upStrategy)
{
    std::scoped_lock lock(m_mutex);
    m_upStrategy = std::move(upStrategy);
}

//----------------------------------------------------------------------------------------------------------------------

Security::State Security::Mediator::GetSecurityState() const
{
    std::shared_lock lock(m_mutex);
    return m_state;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::Mediator::BindPeer(std::shared_ptr<Peer::Proxy> const& spPeerProxy)
{
    std::scoped_lock lock(m_mutex);

    if (!m_upStrategy && !m_upExchangeProcessor) [[unlikely]] {
        throw std::runtime_error("The Security Mediator has not been setup with a security strategy!");
    }

    // We must be provided a peer to track. 
    if (!spPeerProxy) [[unlikely]] {
        throw std::runtime_error("The Security Mediator was not bound to a valid peer!");
    }

    if (m_spPeer) [[unlikely]] {
        throw std::runtime_error("The Security Mediator may only be bound to a peer once!");
    }

    // Capture the bound peer in order to manage the security process and to ensure the bind 
    // method is not called multuple times. 
    m_spPeer = spPeerProxy;

    // If an exchange processor has been setup, set the receiver on the peer to it.
    if (m_upExchangeProcessor) [[likely]] {
        spPeerProxy->SetReceiver(m_upExchangeProcessor.get());
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Security::Mediator::BindSecurityContext(MessageContext& context) const
{
    auto& mutex = m_mutex;
    auto const& upStrategy = m_upStrategy;

    context.BindEncryptionHandlers(
        [&mutex, &upStrategy] (auto buffer, auto nonce) -> Security::Encryptor::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return {}; }
            return upStrategy->Encrypt(buffer, nonce);
        },
        [&mutex, &upStrategy] (auto buffer, auto nonce) -> Security::Decryptor::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return {}; }
            return upStrategy->Decrypt(buffer, nonce);
        });

    context.BindSignatureHandlers(
        [&mutex, &upStrategy] (auto& buffer) -> Security::Signator::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return -1; }
            return upStrategy->Sign(buffer);
        },
        [&mutex, &upStrategy] (auto buffer) -> Security::Verifier::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return Security::VerificationStatus::Failed; }
            return upStrategy->Verify(buffer);
        },
        [&mutex, &upStrategy] () -> Security::SignatureSizeGetter::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upStrategy) [[unlikely]] { return 0; }
            return upStrategy->GetSignatureSize();
        });
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> Security::Mediator::SetupExchangeInitiator(
    Security::Strategy strategy, std::shared_ptr<IConnectProtocol> const& spConnectProtocol)
{
    std::scoped_lock lock(m_mutex);

    // This function should only be called when first creating the mediator, another method 
    // should be used to resynchronize.
    if (m_upStrategy) { return {}; }

    // Make a security strategy with the initial role of an initiator.
    auto upStrategy = Security::CreateStrategy(strategy, Security::Role::Initiator, m_context);

    // Make an ExchangeProcessor for the peer, so handshake messages may be processed. The 
    // processor will use the security strategy to negiotiate keys and initialize its state.
    if (!SetupExchangeProcessor(std::move(upStrategy), spConnectProtocol)) { return {}; }

    auto const [success, request] = m_upExchangeProcessor->Prepare();
    if (!success) { return {}; }

    // Provide the caller the exchange request to be sent to the peer. 
    return request;
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Mediator::SetupExchangeAcceptor(Security::Strategy strategy)
{
    std::scoped_lock lock(m_mutex);

    // This function should only be called when first creating the mediator, another method 
    // should be used to resynchronize.
    if (m_upStrategy) [[unlikely]] { return false; }

    // Make a security strategy with the initial role of an acceptor.
    auto upStrategy = Security::CreateStrategy(strategy, Security::Role::Acceptor, m_context);

    // Make an ExchangeProcessor for the peer, so handshake messages may be processed. The 
    // processor will use the security strategy to negiotiate keys and initialize its state.
    if (!SetupExchangeProcessor(std::move(upStrategy), nullptr)) { return false; }

    auto const [success, request] = m_upExchangeProcessor->Prepare();
    if (!success) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Mediator::SetupExchangeProcessor(
    std::unique_ptr<ISecurityStrategy>&& upStrategy, std::shared_ptr<IConnectProtocol> const& spConnectProtocol)
{
    if (m_upExchangeProcessor || !upStrategy) [[unlikely]] { return false; }

    m_upExchangeProcessor = std::make_unique<ExchangeProcessor>(
        m_spNodeIdentifier, spConnectProtocol, this, std::move(upStrategy));

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
