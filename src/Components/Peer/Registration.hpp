//----------------------------------------------------------------------------------------------------------------------
// File: Registration.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Message/MessageContext.hpp"
#include "Components/Network/Actions.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Peer {
//----------------------------------------------------------------------------------------------------------------------

class Proxy;
class Registration;

//----------------------------------------------------------------------------------------------------------------------
} // Peer namespace
//----------------------------------------------------------------------------------------------------------------------

class Peer::Registration
{
public:
    Registration(
        std::weak_ptr<Peer::Proxy> const& wpProxy,
        Network::Endpoint::Identifier identifier,
        Network::Protocol protocol,
        Network::RemoteAddress const& address = {},
        Network::MessageAction const& messenger = {},
        Network::DisconnectAction const& disconnector = {});

    [[nodiscard]] Message::Context const& GetMessageContext() const;
    [[nodiscard]] Message::Context& GetWritableMessageContext();
    [[nodiscard]] Network::Endpoint::Identifier GetEndpointIdentifier() const;
    [[nodiscard]] Network::Protocol GetEndpointProtocol() const;
    [[nodiscard]] Network::RemoteAddress const& GetAddress() const;
    [[nodiscard]] Network::MessageAction const& GetMessageAction() const;
    [[nodiscard]] Network::DisconnectAction const& GetDisconnectAction() const;

private:
    Message::Context m_context;
    Network::RemoteAddress const m_address;
    Network::MessageAction const m_messenger;
    Network::DisconnectAction const m_disconnector;
};

//----------------------------------------------------------------------------------------------------------------------
