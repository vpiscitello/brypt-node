//----------------------------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "EndpointDefinitions.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/EndpointDefinitions.hpp"
//----------------------------------------------------------------------------------------------------------------------

Network::LoRa::Endpoint::Endpoint(Network::Endpoint::Properties const& properties)
    : IEndpoint(properties)
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::LoRa::Endpoint::~Endpoint()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Network::LoRa::Endpoint::GetProtocol() const
{
    return ProtocolType;
}

//----------------------------------------------------------------------------------------------------------------------

std::string Network::LoRa::Endpoint::GetScheme() const
{
    return Scheme.data();
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
Network::BindingAddress Network::LoRa::Endpoint::GetBinding() const
{
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

void Network::LoRa::Endpoint::Startup()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::Shutdown()
{
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::IsActive() const
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::ScheduleBind([[maybe_unused]] BindingAddress const& binding)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::ScheduleConnect(RemoteAddress const& address)
{
    RemoteAddress remote = address;
    return ScheduleConnect(std::move(remote));
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::ScheduleConnect([[maybe_unused]] RemoteAddress&& address)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::ScheduleConnect(
    [[maybe_unused]] RemoteAddress&& address, [[maybe_unused]] Node::SharedIdentifier const& spIdentifier)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::ScheduleSend(
    [[maybe_unused]] Node::Identifier const& identifier, [[maybe_unused]] std::string&& message)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::ScheduleSend(
    [[maybe_unused]] Node::Identifier const& identifier, [[maybe_unused]] Message::ShareablePack const& spSharedPack)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::LoRa::Endpoint::ScheduleSend(
    [[maybe_unused]] Node::Identifier const& identifier, [[maybe_unused]] MessageVariant&& message)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
