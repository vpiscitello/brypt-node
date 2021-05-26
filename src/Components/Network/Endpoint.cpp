//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Defines a set of communication methods for use on varying types of communication
// protocols. Currently supports TCP sockets.
//----------------------------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
#include "Address.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/LoRa/Endpoint.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
#include "Components/Peer/Proxy.hpp"
//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Network::IEndpoint> Network::Endpoint::Factory(
    Protocol protocol,
    Operation operation,
    std::shared_ptr<Event::Publisher> const& spEventPublisher,
    IEndpointMediator* const pEndpointMediator,
    IPeerMediator* const pPeerMediator)
{
    std::unique_ptr<IEndpoint> upEndpoint;

    switch (protocol) {
        case Protocol::LoRa: {
            upEndpoint = std::make_unique<LoRa::Endpoint>(operation, spEventPublisher);
        } break;
        case Protocol::TCP: {
            upEndpoint = std::make_unique<TCP::Endpoint>(operation, spEventPublisher);
        } break;
        case Protocol::Invalid: assert(false); break;
    }
    assert(upEndpoint);

    if (pEndpointMediator) { upEndpoint->RegisterMediator(pEndpointMediator); }
    if (pPeerMediator) { upEndpoint->RegisterMediator(pPeerMediator); }

    return upEndpoint;
}

//----------------------------------------------------------------------------------------------------------------------

Network::IEndpoint::IEndpoint(
    Protocol protocol, Operation operation, std::shared_ptr<Event::Publisher> const& spEventPublisher)
    : m_identifier(Endpoint::IdentifierGenerator::Instance().Generate())
    , m_protocol(protocol)
    , m_operation(operation)
    , m_binding()
    , m_spEventPublisher(spEventPublisher)
    , m_pEndpointMediator(nullptr)
    , m_pPeerMediator(nullptr)
    , m_optShutdownCause()
{
    assert(m_operation != Operation::Invalid);
    assert(m_spEventPublisher);
    {
        using enum Event::Type;
        spEventPublisher->Advertise({EndpointStarted, EndpointStopped, BindingFailed, ConnectionFailed});
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

void Network::IEndpoint::RegisterMediator(IEndpointMediator* const pMediator)
{
    assert(!IsActive());
    m_pEndpointMediator = pMediator;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::RegisterMediator(IPeerMediator* const pMediator)
{
    assert(!IsActive());
    m_pPeerMediator = pMediator;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnStarted() const
{
    m_spEventPublisher->Publish<Event::Type::EndpointStarted>({ m_identifier, m_protocol, m_operation });
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnStopped() const
{
    m_spEventPublisher->Publish<Event::Type::EndpointStopped>(
        { m_identifier, m_protocol, m_operation, m_optShutdownCause.value_or(ShutdownCause::ShutdownRequest) });
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnBindFailed(BindingAddress const& binding) const
{
    assert(m_operation == Operation::Server);
    SetShutdownCause(ShutdownCause::BindingFailed);
    m_spEventPublisher->Publish<Event::Type::BindingFailed>({ m_identifier, binding });
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnConnectFailed(RemoteAddress const& address) const
{
    assert(m_operation == Operation::Client);
    m_spEventPublisher->Publish<Event::Type::ConnectionFailed>({ m_identifier, address });
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnUnexpectedError() const
{
    SetShutdownCause(ShutdownCause::UnexpectedError);
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::OnBindingUpdated(BindingAddress const& binding)
{
    // If a binding failure was marked as a potential cause of the shutdown, reset the captured shutdown error.
    if (m_optShutdownCause && *m_optShutdownCause == ShutdownCause::BindingFailed) {
        m_optShutdownCause.reset();
    }
    if (m_pEndpointMediator) [[likely]] { m_pEndpointMediator->UpdateBinding(m_identifier, binding); }
}

//----------------------------------------------------------------------------------------------------------------------

void Network::IEndpoint::SetShutdownCause(ShutdownCause cause) const
{
    if (m_optShutdownCause) { return; } // Don't overwrite the initial value of the shutdown error. 
    m_optShutdownCause = cause;
}

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Peer::Proxy> Network::IEndpoint::LinkPeer(
    Node::Identifier const& identifier, RemoteAddress const& address) const
{
    std::shared_ptr<Peer::Proxy> spPeerProxy;

    // If the endpoint has a known peer mediator then we should use the mediator to acquire or 
    // link this endpoint with a unified peer identified by the provided Brypt Identifier. 
    // Otherwise, the endpoint can make a self contained Brypt Peer. Note: This conditional branch
    // should only be hit in unit tests of the endpoint. 
    if(m_pPeerMediator) { spPeerProxy = m_pPeerMediator->LinkPeer(identifier, address); } 
    else { spPeerProxy = std::make_shared<Peer::Proxy>(identifier); }

    return spPeerProxy;
}

//----------------------------------------------------------------------------------------------------------------------
