//------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports ZMQ REQ/REP, ZMQ StreamBridge, and TCP sockets.
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "DirectEndpoint.hpp"
#include "LoRaEndpoint.hpp"
#include "StreamBridgeEndpoint.hpp"
#include "TcpEndpoint.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Message/Message.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

std::unique_ptr<CEndpoint> Endpoints::Factory(
    TechnologyType technology,
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    std::string_view interface,
    Endpoints::OperationType operation,
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator, 
    IMessageSink* const pMessageSink)
{
    switch (technology) {
        case TechnologyType::Direct: {
            return std::make_unique<CDirectEndpoint>(
                spBryptIdentifier, interface, operation,
                pEndpointMediator, pPeerMediator, pMessageSink);
        }
        case TechnologyType::LoRa: {
            return std::make_unique<CLoRaEndpoint>(
                spBryptIdentifier, interface, operation,
                pEndpointMediator, pPeerMediator, pMessageSink);
        }
        case TechnologyType::StreamBridge: {
            return std::make_unique<CStreamBridgeEndpoint>(
                spBryptIdentifier, interface, operation,
                pEndpointMediator, pPeerMediator, pMessageSink);
        }
        case TechnologyType::TCP: {
            return std::make_unique<CTcpEndpoint>(
                spBryptIdentifier, interface, operation,
                pEndpointMediator, pPeerMediator, pMessageSink);
        }
        case TechnologyType::Invalid: return nullptr;
    }
    return nullptr;
}

//------------------------------------------------------------------------------------------------

bool CEndpoint::IsActive() const
{
    return m_active;
}

//------------------------------------------------------------------------------------------------

Endpoints::EndpointIdType CEndpoint::GetEndpointIdentifier() const
{
    return m_identifier;
}

//------------------------------------------------------------------------------------------------

Endpoints::OperationType CEndpoint::GetOperation() const
{
    return m_operation;
}

//------------------------------------------------------------------------------------------------

void CEndpoint::PublishPeerConnection(std::weak_ptr<CBryptPeer> const& wpBryptPeer)
{
    if(m_pPeerMediator) {
        m_pPeerMediator->ForwardConnectionStateChange(
            m_technology, wpBryptPeer, ConnectionState::Connected);
    }
}

//------------------------------------------------------------------------------------------------

void CEndpoint::UnpublishPeerConnection(std::weak_ptr<CBryptPeer> const& wpBryptPeer)
{
    if(m_pPeerMediator) {
        m_pPeerMediator->ForwardConnectionStateChange(
            m_technology, wpBryptPeer, ConnectionState::Disconnected);
    }
}

//------------------------------------------------------------------------------------------------
