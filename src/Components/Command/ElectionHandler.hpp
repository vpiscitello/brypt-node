//------------------------------------------------------------------------------------------------
// File: ElectionHandler.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Elections
//------------------------------------------------------------------------------------------------
class Command::CElectionHandler : public Command::IHandler {
public:
    enum class Phase { Probe, Precommit, Vote, Abort, Results, Close };
    
    explicit CElectionHandler(CBryptNode& instance);

    // IHandler{
    bool HandleMessage(CMessage const& message) override;
    // }IHandler

    bool ProbeHandler();
    bool PrecommitHandler();
    bool VoteHandler();
    bool AbortHandler();
    bool ResultsHandler();
    bool CloseHandler();

};

//------------------------------------------------------------------------------------------------
