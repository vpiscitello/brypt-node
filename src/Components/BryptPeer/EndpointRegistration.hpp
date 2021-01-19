//------------------------------------------------------------------------------------------------
// File: EndpointRegistration.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptMessage/MessageContext.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/MessageScheduler.hpp"
#include "Components/Network/Protocol.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

class EndpointRegistration
{
public:
    EndpointRegistration(
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
    MessageContext m_context;
    MessageScheduler const m_scheduler;
    std::string const m_entry;
};

//------------------------------------------------------------------------------------------------
