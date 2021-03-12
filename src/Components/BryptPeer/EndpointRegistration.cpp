//----------------------------------------------------------------------------------------------------------------------
// File: EndpointRegistration.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "EndpointRegistration.hpp"
//----------------------------------------------------------------------------------------------------------------------

EndpointRegistration::EndpointRegistration(
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

MessageContext const& EndpointRegistration::GetMessageContext() const
{
    return m_context;
}

//----------------------------------------------------------------------------------------------------------------------

MessageContext& EndpointRegistration::GetWritableMessageContext()
{
    return m_context;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Identifier EndpointRegistration::GetEndpointIdentifier() const
{
    return m_context.GetEndpointIdentifier();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol EndpointRegistration::GetEndpointProtocol() const
{
    return m_context.GetEndpointProtocol();
}

//----------------------------------------------------------------------------------------------------------------------

MessageScheduler const& EndpointRegistration::GetScheduler() const
{
    return m_scheduler;
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress const& EndpointRegistration::GetAddress() const
{
    return m_address;
}

//----------------------------------------------------------------------------------------------------------------------
