//----------------------------------------------------------------------------------------------------------------------
// File: Registration.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Registration.hpp"
//----------------------------------------------------------------------------------------------------------------------

Peer::Registration::Registration(
    Network::Endpoint::Identifier identifier,
    Network::Protocol protocol,
    Network::RemoteAddress const& address,
    Network::MessageAction const& messenger,
    Network::DisconnectAction const& disconnector)
    : m_context(identifier, protocol)
    , m_address(address)
    , m_messenger(messenger)
    , m_disconnector(disconnector)
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context const& Peer::Registration::GetMessageContext() const { return m_context; }

//----------------------------------------------------------------------------------------------------------------------

MessageContext& Peer::Registration::GetWritableMessageContext() { return m_context; }

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
