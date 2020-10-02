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
#include "../../BryptMessage/ApplicationMessage.hpp"
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

std::shared_ptr<CBryptPeer> CEndpoint::LinkPeer(BryptIdentifier::CContainer const& identifier) const
{
    std::shared_ptr<CBryptPeer> spBryptPeer = {};

    // If the endpoint has a known peer mediator then we should use the mediator to acquire or 
    // link this endpoint with a unified peer identified by the provided Brypt Identifier. 
    // Otherwise, the endpoint can make a self contained Brypt Peer. Note: This conditional branch
    // should only be hit in unit tests of the endpoint. 
    if(m_pPeerMediator) {
        spBryptPeer = m_pPeerMediator->LinkPeer(identifier);
    } else {
        spBryptPeer = std::make_shared<CBryptPeer>(identifier);
    }

    return spBryptPeer;
}

//------------------------------------------------------------------------------------------------
