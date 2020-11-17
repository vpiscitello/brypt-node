//------------------------------------------------------------------------------------------------
// File: SecurityMediator.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "SecurityMediator.hpp"
#include "SecurityUtils.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../MessageControl/ExchangeProcessor.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../Interfaces/ConnectProtocol.hpp"
#include "../../Interfaces/SecurityStrategy.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: 
//------------------------------------------------------------------------------------------------
CSecurityMediator::CSecurityMediator(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    Security::Context context,
    std::weak_ptr<IMessageSink> const& wpAuthorizedSink)
    : m_mutex()
    , m_context(context)
    , m_state(Security::State::Unauthorized)
    , m_spBryptIdentifier(spBryptIdentifier)
    , m_spBryptPeer()
    , m_upSecurityStrategy()
    , m_upExchangeProcessor()
    , m_wpAuthorizedSink(wpAuthorizedSink)
{
}

//------------------------------------------------------------------------------------------------

CSecurityMediator::CSecurityMediator(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy)
    : m_mutex()
    , m_context(upSecurityStrategy->GetContextType())
    , m_state(Security::State::Unauthorized)
    , m_spBryptIdentifier(spBryptIdentifier)
    , m_spBryptPeer()
    , m_upSecurityStrategy(std::move(upSecurityStrategy))
    , m_upExchangeProcessor()
    , m_wpAuthorizedSink()
{
}

//------------------------------------------------------------------------------------------------

CSecurityMediator::~CSecurityMediator()
{
    // If the unauthorized sink is still active for the peer, we must unset the peer's receiver
    // to ensure the receiver does not point to invalid memory. Note: the process of acquiring the 
    // receiver mutex in CBryptPeer should ensure that the sink is not destructed while it is 
    // actively processing a message. 
    if (m_spBryptPeer && m_upExchangeProcessor) {
        m_spBryptPeer->SetReceiver(nullptr);
    }
}

//------------------------------------------------------------------------------------------------

void CSecurityMediator::HandleExchangeClose(
    ExchangeStatus status,
    std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy)
{
    std::scoped_lock lock(m_mutex);

    if (!m_spBryptPeer) {
        return;
    }    

    switch (status) {
        // If we have been notified us of a successful exchange set the message sink for the peer 
        // to the authorized sink and mark the peer as authorized. 
        case ExchangeStatus::Success: {
            if (auto const spMessageSink = m_wpAuthorizedSink.lock(); spMessageSink) {
                m_state = Security::State::Authorized;
                m_upSecurityStrategy = std::move(upSecurityStrategy);
                m_spBryptPeer->SetReceiver(spMessageSink.get());
            } else {
                assert(false); // We should always be able to acquire the authorized sink 
            }
        } break;
        // If we have been notified us of a failed exchange unset the message sink for the peer 
        // and mark the peer as unauthorized. 
        case ExchangeStatus::Failed: {
            m_state = Security::State::Unauthorized;
            m_spBryptPeer->SetReceiver(nullptr);
        } break;
        default: assert(false); break;  // What is this?
    }

    m_upExchangeProcessor.reset(); // Delete the exchange processor. 
}

//------------------------------------------------------------------------------------------------

Security::State CSecurityMediator::GetSecurityState() const
{
    std::shared_lock lock(m_mutex);
    return m_state;
}

//------------------------------------------------------------------------------------------------

void CSecurityMediator::BindPeer(std::shared_ptr<CBryptPeer> const& spBryptPeer)
{
    std::scoped_lock lock(m_mutex);

    if (!m_upSecurityStrategy && !m_upExchangeProcessor) {
        throw std::runtime_error("The Security Mediator has not been setup with a security strategy!");
    }

    // We must be provided a peer to track. 
    if (!spBryptPeer) {
        throw std::runtime_error("The Security Mediator was not bound to a valid peer!");
    }

    if (m_spBryptPeer) {
        throw std::runtime_error("The Security Mediator may only be bound to a peer once!");
    }

    // Capture the bound peer in order to manage the security process and to ensure the bind 
    // method is not called multuple times. 
    m_spBryptPeer = spBryptPeer;

    // If an exchange processor has been setup, set the receiver on the peer to it.
    if (m_upExchangeProcessor) {
        spBryptPeer->SetReceiver(m_upExchangeProcessor.get());
    }
}

//------------------------------------------------------------------------------------------------

void CSecurityMediator::BindSecurityContext(CMessageContext& context) const
{
    auto& mutex = m_mutex;
    auto const& upSecurityStrategy = m_upSecurityStrategy;

    context.BindEncryptionHandlers(
        [&mutex, &upSecurityStrategy] (
            auto const& buffer, auto size, auto nonce) -> Security::Encryptor::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upSecurityStrategy) {
                return {};
            }
            return upSecurityStrategy->Encrypt(buffer, size, nonce);
        },
        [&mutex, &upSecurityStrategy] (
            auto const& buffer, auto size, auto nonce) -> Security::Decryptor::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upSecurityStrategy) {
                return {};
            }
            return upSecurityStrategy->Decrypt(buffer, size, nonce);
        });

    context.BindSignatureHandlers(
        [&mutex, &upSecurityStrategy] (auto& buffer) -> Security::Signator::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upSecurityStrategy) {
                return -1;
            }
            return upSecurityStrategy->Sign(buffer);
        },
        [&mutex, &upSecurityStrategy] (auto const& buffer) -> Security::Verifier::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upSecurityStrategy) {
                return Security::VerificationStatus::Failed;
            }
            return upSecurityStrategy->Verify(buffer);
        },
        [&mutex, &upSecurityStrategy] () -> Security::SignatureSizeGetter::result_type
        {
            std::scoped_lock lock(mutex);
            if (!upSecurityStrategy) {
                return 0;
            }
            return upSecurityStrategy->GetSignatureSize();
        });
}

//------------------------------------------------------------------------------------------------

std::optional<std::string> CSecurityMediator::SetupExchangeInitiator(
    Security::Strategy strategy,
    std::shared_ptr<IConnectProtocol> const& spConnectProtocol)
{
    std::scoped_lock lock(m_mutex);

    // This function should only be called when first creating the mediator, another method 
    // should be used to resynchronize.
    if (m_upSecurityStrategy) {
        return {};
    }

    // Make a security strategy with the initial role of an initiator.
    auto upSecurityStrategy = Security::CreateStrategy(
        strategy, Security::Role::Initiator, m_context);

    // Make an ExchangeProcessor for the peer, so handshake messages may be processed. The 
    // processor will use the security strategy to negiotiate keys and initialize its state.
    if (!SetupExchangeProcessor(std::move(upSecurityStrategy), spConnectProtocol)) {
        return {};
    }

    auto const [success, request] = m_upExchangeProcessor->Prepare();
    if (!success) {
        return {};
    }

    // Provide the caller the exchange request to be sent to the peer. 
    return request;
}

//------------------------------------------------------------------------------------------------

bool CSecurityMediator::SetupExchangeAcceptor(Security::Strategy strategy)
{
    std::scoped_lock lock(m_mutex);

    // This function should only be called when first creating the mediator, another method 
    // should be used to resynchronize.
    if (m_upSecurityStrategy) {
        return false;
    }

    // Make a security strategy with the initial role of an acceptor.
    auto upSecurityStrategy = Security::CreateStrategy(
        strategy, Security::Role::Acceptor, m_context);

    // Make an ExchangeProcessor for the peer, so handshake messages may be processed. The 
    // processor will use the security strategy to negiotiate keys and initialize its state.
    if (!SetupExchangeProcessor(std::move(upSecurityStrategy), nullptr)) {
        return false;
    }

    auto const [success, request] = m_upExchangeProcessor->Prepare();
    if (!success) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------

bool CSecurityMediator::SetupExchangeProcessor(
    std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy,
    std::shared_ptr<IConnectProtocol> const& spConnectProtocol)
{
    if (m_upExchangeProcessor || !upSecurityStrategy) {
        return false;    
    }

    m_upExchangeProcessor = std::make_unique<CExchangeProcessor>(
        m_spBryptIdentifier, spConnectProtocol, this, std::move(upSecurityStrategy));

    return true;
}

//------------------------------------------------------------------------------------------------
