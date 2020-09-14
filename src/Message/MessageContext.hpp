//------------------------------------------------------------------------------------------------
// File: MessageContext.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/EndpointIdentifier.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: A class to describe various information about the message context local to the 
// node itself. This information is not a part of the message, but determined by the endpoint
// it was received or transmitted by. Currently, this is being used to identify which endpoint 
// the a response should be forwarded to. This is needed because it is valid for a peer to be 
// connected via multiple endpoints (e.g. connected as a server and a client).
//------------------------------------------------------------------------------------------------
class CMessageContext {
public:
	CMessageContext();
	CMessageContext(Endpoints::EndpointIdType identifier, Endpoints::TechnologyType technology);

	Endpoints::EndpointIdType GetEndpointIdentifier() const;
	Endpoints::TechnologyType GetEndpointTechnology() const;

private:
	Endpoints::EndpointIdType m_endpointIdentifier;
	Endpoints::TechnologyType m_endpointTechnology;

};

//------------------------------------------------------------------------------------------------
