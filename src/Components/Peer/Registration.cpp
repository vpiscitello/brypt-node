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
    MessageScheduler const& scheduler)
    : m_context(identifier, protocol)
    , m_scheduler(scheduler)
    , m_address(address)
{
}

//----------------------------------------------------------------------------------------------------------------------

MessageContext const& Peer::Registration::GetMessageContext() const
{
    return m_context;
}

//----------------------------------------------------------------------------------------------------------------------

MessageContext& Peer::Registration::GetWritableMessageContext()
{
    return m_context;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Identifier Peer::Registration::GetEndpointIdentifier() const
{
    return m_context.GetEndpointIdentifier();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Peer::Registration::GetEndpointProtocol() const
{
    return m_context.GetEndpointProtocol();
}

//----------------------------------------------------------------------------------------------------------------------

MessageScheduler const& Peer::Registration::GetScheduler() const
{
    return m_scheduler;
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress const& Peer::Registration::GetAddress() const
{
    return m_address;
}

//----------------------------------------------------------------------------------------------------------------------
