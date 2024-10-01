//----------------------------------------------------------------------------------------------------------------------
// File: Registration.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Registration.hpp"
#include "Components/Peer/Proxy.hpp"
//----------------------------------------------------------------------------------------------------------------------

Peer::Registration::Registration(
    std::weak_ptr<Peer::Proxy> const& wpProxy,
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    Network::RemoteAddress const& address,
    Network::MessageAction const& messenger,
    Network::DisconnectAction const& disconnector)
    : m_context(wpProxy, identifier, protocol)
    , m_address(address)
    , m_messenger(messenger)
    , m_disconnector(disconnector)
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context const& Peer::Registration::GetMessageContext() const { return m_context; }

//----------------------------------------------------------------------------------------------------------------------

Message::Context& Peer::Registration::GetWritableMessageContext() { return m_context; }

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Identifier Peer::Registration::GetEndpointIdentifier() const
{
    return m_context.GetEndpointIdentifier();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Peer::Registration::GetEndpointProtocol() const { return m_context.GetEndpointProtocol(); }

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress const& Peer::Registration::GetAddress() const { return m_address; }

//----------------------------------------------------------------------------------------------------------------------

Network::MessageAction const& Peer::Registration::GetMessageAction() const { return m_messenger; }

//----------------------------------------------------------------------------------------------------------------------

Network::DisconnectAction const& Peer::Registration::GetDisconnectAction() const { return m_disconnector; }

//----------------------------------------------------------------------------------------------------------------------
