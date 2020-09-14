//------------------------------------------------------------------------------------------------
// File: MessageContext.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
//------------------------------------------------------------------------------------------------

CMessageContext::CMessageContext()
	: m_endpointIdentifier(Endpoints::InvalidEndpointIdentifier)
	, m_endpointTechnology(Endpoints::TechnologyType::Invalid)
{
}

CMessageContext::CMessageContext(
	Endpoints::EndpointIdType identifier,
	Endpoints::TechnologyType technology)
	: m_endpointIdentifier(identifier)
	, m_endpointTechnology(technology)
{
}

Endpoints::EndpointIdType CMessageContext::GetEndpointIdentifier() const
{
	return m_endpointIdentifier;
}

Endpoints::TechnologyType CMessageContext::GetEndpointTechnology() const
{
	return m_endpointTechnology;
}

//------------------------------------------------------------------------------------------------
