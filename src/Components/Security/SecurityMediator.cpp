//------------------------------------------------------------------------------------------------
// File: SecurityMediator.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "SecurityMediator.hpp"
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
    std::unique_ptr<ISecurityStrategy>&& upSecurityStrategy,
    std::weak_ptr<IMessageSink> const& wpAuthorizedSink)
    : m_state(SecurityState::Unauthorized)
    , m_upSecurityStrategy(std::move(upSecurityStrategy))
    , m_spBryptPeer()
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
                    m_state = SecurityState::Authorized;
                    m_upSecurityStrategy = std::move(upSecurityStrategy);
                    m_spBryptPeer->SetReceiver(spMessageSink.get());
                } else {
                    assert(false); // We should always be able to acquire the authorized sink 
                }
            } break;
            // If we have been notified us of a failed exchange unset the message sink for the peer 
            // and mark the peer as unauthorized. 
            case ExchangeStatus::Failed: {
                m_state = SecurityState::Unauthorized;
                m_spBryptPeer->SetReceiver(nullptr);
            } break;
            default: assert(false); break;  // What is this?
        }

        m_upExchangeProcessor.reset(); // Delete the exchange processor. 
    }    
}

//------------------------------------------------------------------------------------------------

SecurityState CSecurityMediator::GetSecurityState() const
{
    return m_state;
}

//------------------------------------------------------------------------------------------------

void CSecurityMediator::Bind(std::shared_ptr<CBryptPeer> const& spBryptPeer)
{
    // We must be provided a peer to track. 
    if (!spBryptPeer) {
        return; 
    }

    if (m_spBryptPeer) {
        throw std::runtime_error("The Security Mediator may only be bound to a Brypt Peer once!");
    }

    // Capture the peer that has attached us. 
    m_spBryptPeer = spBryptPeer;

    // Now that we have been attached to a peer, the peer must be prepared for a key exchange.
    PrepareExchange();
}

//------------------------------------------------------------------------------------------------

void CSecurityMediator::PrepareExchange()
{
    assert(m_spBryptPeer);  // This function should only be called if a peer has been attached. 
    // Make an ExchangeProcessor for the peer, so handshake messages may be processed. The 
    // processor will use the security strategy to negiotiate keys and initialize its state.
    m_upExchangeProcessor = std::make_unique<CExchangeProcessor>(
        this, std::move(m_upSecurityStrategy));
    m_spBryptPeer->SetReceiver(m_upExchangeProcessor.get());
}

//------------------------------------------------------------------------------------------------
