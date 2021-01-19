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
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        MessageScheduler const& scheduler = {},
        std::string_view uri = {});

    [[nodiscard]] MessageContext const& GetMessageContext() const;
    [[nodiscard]] MessageContext& GetWritableMessageContext();
    [[nodiscard]] Network::Endpoint::Identifier GetEndpointIdentifier() const;
    [[nodiscard]] Network::Protocol GetEndpointProtocol() const;
    [[nodiscard]] MessageScheduler const& GetScheduler() const;
    [[nodiscard]] std::string const& GetEntry() const;

private:
    MessageContext m_context;
    MessageScheduler const m_scheduler;
    std::string const m_entry;
};

//------------------------------------------------------------------------------------------------
