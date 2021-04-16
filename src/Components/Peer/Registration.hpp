//----------------------------------------------------------------------------------------------------------------------
// File: Registration.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptMessage/MessageContext.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/MessageScheduler.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Registration;

//----------------------------------------------------------------------------------------------------------------------
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Registration
{
public:
    Registration(
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        Network::RemoteAddress const& address = {},
        MessageScheduler const& scheduler = {});

    [[nodiscard]] MessageContext const& GetMessageContext() const;
    [[nodiscard]] MessageContext& GetWritableMessageContext();
    [[nodiscard]] Network::Endpoint::Identifier GetEndpointIdentifier() const;
    [[nodiscard]] Network::Protocol GetEndpointProtocol() const;
    [[nodiscard]] Network::RemoteAddress const& GetAddress() const;
    [[nodiscard]] MessageScheduler const& GetScheduler() const;

private:
    MessageContext m_context;
    MessageScheduler const m_scheduler;
    Network::RemoteAddress const m_address;
};

//----------------------------------------------------------------------------------------------------------------------
