//------------------------------------------------------------------------------------------------
// File: TransformHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "TransformHandler.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
Command::CTransformHandler::CTransformHandler(CBryptNode& instance)
    : IHandler(Command::Type::Transform, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Transform message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CTransformHandler::HandleMessage(AssociatedMessage const& associatedMessage)
{
    bool status = false;

    auto& [spBryptPeer, message] = associatedMessage;
    auto const phase = static_cast<CTransformHandler::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::Information: {
            status = InfoHandler();
        } break;
        case Phase::Host: {
            status = HostHandler();
        } break;
        case Phase::Connect: {
            status = ConnectHandler();
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
bool Command::CTransformHandler::InfoHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CTransformHandler::HostHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CTransformHandler::ConnectHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CTransformHandler::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------
