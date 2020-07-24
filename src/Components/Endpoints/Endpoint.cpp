//------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports ZMQ REQ/REP, ZMQ StreamBridge, and TCP sockets.
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "Peer.hpp"
#include "DirectEndpoint.hpp"
#include "LoRaEndpoint.hpp"
#include "StreamBridgeEndpoint.hpp"
#include "TcpEndpoint.hpp"
#include "../../Message/Message.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

std::unique_ptr<CEndpoint> Endpoints::Factory(
    TechnologyType technology,
    NodeUtils::NodeIdType id,
    std::string_view interface,
    Endpoints::OperationType operation,
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator, 
    IMessageSink* const pMessageSink)
{
    switch (technology) {
        case TechnologyType::Direct: {
            return std::make_unique<CDirectEndpoint>(
                id, interface, operation, pEndpointMediator, pPeerMediator, pMessageSink);
        }
        case TechnologyType::LoRa: {
            return std::make_unique<CLoRaEndpoint>(
                id, interface, operation, pEndpointMediator, pPeerMediator, pMessageSink);
        }
        case TechnologyType::StreamBridge: {
            return std::make_unique<CStreamBridgeEndpoint>(
                id, interface, operation, pEndpointMediator, pPeerMediator, pMessageSink);
        }
        case TechnologyType::TCP: {
            return std::make_unique<CTcpEndpoint>(
                id, interface, operation, pEndpointMediator, pPeerMediator, pMessageSink);
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

Endpoints::EndpointIdType CEndpoint::GetIdentifier() const
{
    return m_identifier;
}

//------------------------------------------------------------------------------------------------

Endpoints::OperationType CEndpoint::GetOperation() const
{
    return m_operation;
}

//------------------------------------------------------------------------------------------------

void CEndpoint::PublishPeerConnection(CPeer const& peer)
{
    if (m_pMessageSink) {
        m_pMessageSink->PublishPeerConnection(m_identifier, peer.GetNodeId());
    }

    if(m_pPeerMediator) {
        m_pPeerMediator->ForwardPeerConnectionStateChange(peer, ConnectionState::Connected);
    }
}

//------------------------------------------------------------------------------------------------

void CEndpoint::UnpublishPeerConnection(CPeer const& peer)
{
    if (m_pMessageSink) {
        m_pMessageSink->UnpublishPeerConnection(m_identifier, peer.GetNodeId());
    }

    if(m_pPeerMediator) {
        m_pPeerMediator->ForwardPeerConnectionStateChange(peer, ConnectionState::Disconnected);
    }
}

//------------------------------------------------------------------------------------------------
