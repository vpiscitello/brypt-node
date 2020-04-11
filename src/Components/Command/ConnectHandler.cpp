//------------------------------------------------------------------------------------------------
// File: ConnectHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ConnectHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Endpoints/Endpoint.hpp"
#include "../BryptNode/BryptNode.hpp"
#include "../BryptNode/NetworkState.hpp"
#include "../BryptNode/NodeState.hpp"
#include "../Endpoints/Endpoint.hpp"
#include <chrono>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Command::CConnectHandler::CConnectHandler(CBryptNode& instance)
    : IHandler(Command::Type::Connect, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Connect message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::HandleMessage(CMessage const& message)
{
    bool status = false;

    auto const phase = static_cast<CConnectHandler::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::Contact: {
            status = ContactHandler();
        } break;
        case Phase::Join: {
            status = JoinHandler();
        } break;
        case Phase::Close: {
            status = CloseHandler();
        } break;
        default: break;
    }

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::ContactHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the join phase for the Connect type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::JoinHandler()
{
    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------
