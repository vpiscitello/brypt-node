//------------------------------------------------------------------------------------------------
// File: EndpointRegistration.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Endpoints/EndpointIdentifier.hpp"
#include "../Endpoints/MessageScheduler.hpp"
#include "../Endpoints/TechnologyType.hpp"
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

    Endpoints::EndpointIdType GetEndpointIdentifier() const;
    Endpoints::TechnologyType GetEndpointTechnology() const;
    MessageScheduler const& GetScheduler() const;
    std::string const& GetEntry() const;

private:
    Endpoints::EndpointIdType const m_identifier;
    Endpoints::TechnologyType const m_technology;
    MessageScheduler const m_scheduler;
    std::string const m_entry;
};

//------------------------------------------------------------------------------------------------
