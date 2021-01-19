//------------------------------------------------------------------------------------------------
// File: Events.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/EndpointTypes.hpp"
#include "Utilities/NetworkUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <boost/asio/ip/tcp.hpp>
//------------------------------------------------------------------------------------------------
#include <memory>
#include <string>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Network::TCP {
//------------------------------------------------------------------------------------------------

class Session;

struct InstructionEvent;
struct DispatchEvent;

//------------------------------------------------------------------------------------------------
} // TCP namespace
//------------------------------------------------------------------------------------------------

struct Network::TCP::InstructionEvent
{
    BryptIdentifier::SharedContainer identifier;
    Instruction instruction;
    NetworkUtils::Address address;
    NetworkUtils::Port port;
};

//------------------------------------------------------------------------------------------------

struct Network::TCP::DispatchEvent
{
    DispatchEvent(std::shared_ptr<Session> const& session, std::string_view message);
    std::shared_ptr<Session> session;
    std::string message;
};

//------------------------------------------------------------------------------------------------
