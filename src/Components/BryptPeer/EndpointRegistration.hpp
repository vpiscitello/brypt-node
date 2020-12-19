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

    [[nodiscard]] CMessageContext const& GetMessageContext() const;
    [[nodiscard]] CMessageContext& GetWritableMessageContext();
    [[nodiscard]] Endpoints::EndpointIdType GetEndpointIdentifier() const;
    [[nodiscard]] Endpoints::TechnologyType GetEndpointTechnology() const;
    [[nodiscard]] MessageScheduler const& GetScheduler() const;
    [[nodiscard]] std::string const& GetEntry() const;

private:
    CMessageContext m_context;
    MessageScheduler const m_scheduler;
    std::string const m_entry;
};

//------------------------------------------------------------------------------------------------
