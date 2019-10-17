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
class Command::CElection : public Command::CHandler {
public:
    CElection(CNode& instance, std::weak_ptr<CState> const& state);
    ~CElection() override {};

    void whatami() override;
    bool HandleMessage(CMessage const& message) override;

    bool ProbeHandler();
    bool PrecommitHandler();
    bool VoteHandler();
    bool AbortHandler();
    bool ResultsHandler();
    bool CloseHandler();

private:
    enum class Phase { PROBE, PRECOMMIT, VOTE, ABORT, RESULTS, CLOSE };
};

//------------------------------------------------------------------------------------------------
