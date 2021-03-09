//------------------------------------------------------------------------------------------------
// File: Election.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Election.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/BryptNode.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Handler::Election::Election(BryptNode& instance)
    : IHandler(Handler::Type::Election, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Election message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Election::HandleMessage(
    AssociatedMessage const& associatedMessage)
{
    bool status = false;

    auto& [wpBryptPeer, message] = associatedMessage;
    auto const phase = static_cast<Election::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::Probe: { status = ProbeHandler(); } break;
        case Phase::Precommit: { status = PrecommitHandler(); } break;
        case Phase::Vote: { status = VoteHandler(); } break;
        case Phase::Abort: { status = AbortHandler(); } break;
        case Phase::Results: { status = ResultsHandler(); } break;
        case Phase::Close: { status = CloseHandler(); } break;
        default: break;
    }

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Election::ProbeHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Election::PrecommitHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Election::VoteHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Election::AbortHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Election::ResultsHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Election::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------
