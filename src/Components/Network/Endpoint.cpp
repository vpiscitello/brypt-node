//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Defines a set of communication methods for use on varying types of communication
// protocols. Currently supports TCP sockets.
//----------------------------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "Address.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components//BryptPeer/BryptPeer.hpp"
#include "Components/Network/LoRa/Endpoint.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Network::IEndpoint> Network::Endpoint::Factory(
    Protocol protocol,
    Operation operation,
    IEndpointMediator const* const pEndpointMediator,
    IPeerMediator* const pPeerMediator)
{
    std::unique_ptr<IEndpoint> upEndpoint;

    switch (protocol) {
        case Protocol::LoRa: {
            upEndpoint = std::make_unique<LoRa::Endpoint>(operation);
        } break;
        case Protocol::TCP: {
            upEndpoint = std::make_unique<TCP::Endpoint>(operation);
        } break;
        case Protocol::Invalid: assert(false); break;
    }

    if (upEndpoint) {
        if (pEndpointMediator) { upEndpoint->RegisterMediator(pEndpointMediator); }
        if (pPeerMediator) { upEndpoint->RegisterMediator(pPeerMediator); }
    }

    return upEndpoint;
}

//----------------------------------------------------------------------------------------------------------------------

Network::IEndpoint::IEndpoint(
    Operation operation, Network::Protocol protocol)
    : m_identifier(Network::Endpoint::IdentifierGenerator::Instance().Generate())
    , m_operation(operation)
    , m_protocol(protocol)
    , m_binding()
    , m_pEndpointMediator(nullptr)
    , m_pPeerMediator(nullptr)
{
    if (m_operation == Operation::Invalid) {
        throw std::runtime_error("Endpoint must be provided and endpoint operation type!");
    }
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Identifier Network::IEndpoint::GetEndpointIdentifier() const
{
    return m_identifier;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Operation Network::IEndpoint::GetOperation() const
{
    return m_operation;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::RegisterMediator(IEndpointMediator const* const pMediator)
{
    if (IsActive()) {
        throw std::runtime_error(
            "Mediator was attempted to be added while the endpoint is active!");
    }

    m_pEndpointMediator = pMediator;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::RegisterMediator(IPeerMediator* const pMediator)
{
    if (IsActive()) {
        throw std::runtime_error(
            "Mediator was attempted to be added while the endpoint is active!");
    }

    m_pPeerMediator = pMediator;
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<BryptPeer> Network::IEndpoint::LinkPeer(
    BryptIdentifier::Container const& identifier, RemoteAddress const& address) const
{
    std::shared_ptr<BryptPeer> spBryptPeer = {};

    // If the endpoint has a known peer mediator then we should use the mediator to acquire or 
    // link this endpoint with a unified peer identified by the provided Brypt Identifier. 
    // Otherwise, the endpoint can make a self contained Brypt Peer. Note: This conditional branch
    // should only be hit in unit tests of the endpoint. 
    if(m_pPeerMediator) {
        spBryptPeer = m_pPeerMediator->LinkPeer(identifier, address);
    } else {
        spBryptPeer = std::make_shared<BryptPeer>(identifier);
    }

    return spBryptPeer;
}

//----------------------------------------------------------------------------------------------------------------------
