//------------------------------------------------------------------------------------------------
// File: EndpointRegistration.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Endpoints/EndpointIdentifier.hpp"
#include "../Endpoints/MessageScheduler.hpp"
#include "../Endpoints/TechnologyType.hpp"
#include "../../BryptMessage/MessageContext.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

class CEndpointRegistration
{
public:
    CEndpointRegistration(
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        MessageScheduler const& scheduler = {},
        std::string_view uri = {});

    CMessageContext const& GetMessageContext() const;
    CMessageContext& GetWritableMessageContext();
    Endpoints::EndpointIdType GetEndpointIdentifier() const;
    Endpoints::TechnologyType GetEndpointTechnology() const;
    MessageScheduler const& GetScheduler() const;
    std::string const& GetEntry() const;

private:
    CMessageContext m_context;
    MessageScheduler const m_scheduler;
    std::string const m_entry;

};

//------------------------------------------------------------------------------------------------
