//------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports TCP sockets.
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "LoRaEndpoint.hpp"
#include "TcpEndpoint.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

std::unique_ptr<CEndpoint> Endpoints::Factory(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    TechnologyType technology,
    std::string_view interface,
    Endpoints::OperationType operation,
    IEndpointMediator const* const pEndpointMediator,
    std::shared_ptr<IPeerMediator> const& spPeerMediator)
{
    switch (technology) {
        case TechnologyType::LoRa: {
            return std::make_unique<CLoRaEndpoint>(
                spBryptIdentifier, interface, operation, pEndpointMediator, spPeerMediator);
        }
        case TechnologyType::TCP: {
            return std::make_unique<CTcpEndpoint>(
                spBryptIdentifier, interface, operation, pEndpointMediator, spPeerMediator);
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

std::shared_ptr<CBryptPeer> CEndpoint::LinkPeer(
    BryptIdentifier::CContainer const& identifier,
    std::string_view uri) const
{
    std::shared_ptr<CBryptPeer> spBryptPeer = {};

    // If the endpoint has a known peer mediator then we should use the mediator to acquire or 
    // link this endpoint with a unified peer identified by the provided Brypt Identifier. 
    // Otherwise, the endpoint can make a self contained Brypt Peer. Note: This conditional branch
    // should only be hit in unit tests of the endpoint. 
    if(m_spPeerMediator) {
        spBryptPeer = m_spPeerMediator->LinkPeer(identifier, uri);
    } else {
        spBryptPeer = std::make_shared<CBryptPeer>(identifier);
    }

    return spBryptPeer;
}

//------------------------------------------------------------------------------------------------
