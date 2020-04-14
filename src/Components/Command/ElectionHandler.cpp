//------------------------------------------------------------------------------------------------
// File: ElectionHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ElectionHandler.hpp"
#include "../../BryptNode/BryptNode.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Command::CElectionHandler::CElectionHandler(CBryptNode& instance)
    : IHandler(Command::Type::Election, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Election message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CElectionHandler::HandleMessage(CMessage const& message) {
    bool status = false;

    auto const phase = static_cast<CElectionHandler::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::Probe: {
            status = ProbeHandler();
        } break;
        case Phase::Precommit: {
            status = PrecommitHandler();
        } break;
        case Phase::Vote: {
            status = VoteHandler();
        } break;
        case Phase::Abort: {
            status = AbortHandler();
        } break;
        case Phase::Results: {
            status = ResultsHandler();
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
bool Command::CElectionHandler::ProbeHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CElectionHandler::PrecommitHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CElectionHandler::VoteHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CElectionHandler::AbortHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CElectionHandler::ResultsHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CElectionHandler::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------
