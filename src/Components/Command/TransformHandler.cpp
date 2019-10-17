//------------------------------------------------------------------------------------------------
// File: TransformHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "TransformHandler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
Command::CTransform::CTransform(CNode& instance, std::weak_ptr<CState> const& state)
    : CHandler(instance, state)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
void Command::CTransform::whatami()
{
    printo("Handling response to Transform request", NodeUtils::PrintType::COMMAND);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Transform message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CTransform::HandleMessage(CMessage const& message)
{
    bool status = false;
    whatami();

    auto const phase = static_cast<CTransform::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::INFO: {
            status = InfoHandler();
        } break;
        case Phase::HOST: {
            status = HostHandler();
        } break;
        case Phase::CONNECT: {
            status = ConnectHandler();
        } break;
        case Phase::CLOSE: {
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
bool Command::CTransform::InfoHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CTransform::HostHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CTransform::ConnectHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CTransform::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------
