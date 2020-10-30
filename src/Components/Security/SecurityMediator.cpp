//------------------------------------------------------------------------------------------------
// File: SecurityMediator.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "SecurityMediator.hpp"
#include "SecurityUtils.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../MessageControl/ExchangeProcessor.hpp"
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
    : m_context(context)
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
    std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy,
    std::weak_ptr<IMessageSink> const& wpAuthorizedSink)
    : m_context(upSecurityStrategy->GetContextType())
    , m_state(Security::State::Unauthorized)
    , m_spBryptIdentifier(spBryptIdentifier)
    , m_spBryptPeer()
    , m_upSecurityStrategy(std::move(upSecurityStrategy))
    , m_upExchangeProcessor()
    , m_wpAuthorizedSink(wpAuthorizedSink)
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
    if (m_spBryptPeer) {
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
}

//------------------------------------------------------------------------------------------------

Security::State CSecurityMediator::GetSecurityState() const
{
    return m_state;
}

//------------------------------------------------------------------------------------------------

void CSecurityMediator::Bind(std::shared_ptr<CBryptPeer> const& spBryptPeer)
{
    if (!m_upSecurityStrategy && !m_upExchangeProcessor) {
        throw std::runtime_error("The Security Mediator has not been setup with a security strategy!");
    }

    // We must be provided a peer to track. 
    if (!spBryptPeer) {
        return; 
    }

    if (m_spBryptPeer) {
        throw std::runtime_error("The Security Mediator may only be bound to a Brypt Peer once!");
    }

    // Make an exchange processor, if one does not exist. This should only be called in test cases
    // where the security strategy has been provided to us manually. Otherwise, it is expected
    // that the caller has first called the appriopiate exchange setup method. 
    if (!m_upExchangeProcessor) {
        m_upExchangeProcessor = std::make_unique<CExchangeProcessor>(
            spBryptPeer->GetBryptIdentifier(), this, std::move(m_upSecurityStrategy));
    }

    // Set the receiver for the provided peer to the exchange processor. 
    spBryptPeer->SetReceiver(m_upExchangeProcessor.get());

    // Capture the bound peer in order to manage the security process and to ensure the bind 
    // method is not called multuple times. 
    m_spBryptPeer = spBryptPeer; 
}

//------------------------------------------------------------------------------------------------

std::optional<std::string> CSecurityMediator::SetupExchangeInitiator(Security::Strategy strategy)
{
    // This function should only be called when first creating the mediator, another method 
    // should be used to resynchronize.
    if (m_upSecurityStrategy) {
        return {};
    }

    // Make a security strategy with the initial role of an initiator.
    m_upSecurityStrategy = Security::CreateStrategy(strategy, Security::Role::Initiator, m_context);

    // Make an ExchangeProcessor for the peer, so handshake messages may be processed. The 
    // processor will use the security strategy to negiotiate keys and initialize its state.
    m_upExchangeProcessor = std::make_unique<CExchangeProcessor>(
        m_spBryptIdentifier, this, std::move(m_upSecurityStrategy));

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
    // This function should only be called when first creating the mediator, another method 
    // should be used to resynchronize.
    if (m_upSecurityStrategy) {
        return false;
    }

    // Make a security strategy with the initial role of an acceptor.
    m_upSecurityStrategy = Security::CreateStrategy(strategy, Security::Role::Acceptor, m_context);

    // Make an ExchangeProcessor for the peer, so handshake messages may be processed. The 
    // processor will use the security strategy to negiotiate keys and initialize its state.
    m_upExchangeProcessor = std::make_unique<CExchangeProcessor>(
        m_spBryptIdentifier, this, std::move(m_upSecurityStrategy));

    auto const [success, request] = m_upExchangeProcessor->Prepare();
    if (!success) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------
