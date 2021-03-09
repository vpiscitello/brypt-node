//------------------------------------------------------------------------------------------------
// File: Election.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Elections
//------------------------------------------------------------------------------------------------
class Handler::Election : public Handler::IHandler
{
public:
    enum class Phase : std::uint8_t { Probe, Precommit, Vote, Abort, Results, Close };
    
    explicit Election(BryptNode& instance);

    // IHandler{
    bool HandleMessage(AssociatedMessage const& associatedMessage) override;
    // }IHandler

    bool ProbeHandler();
    bool PrecommitHandler();
    bool VoteHandler();
    bool AbortHandler();
    bool ResultsHandler();
    bool CloseHandler();
};

//------------------------------------------------------------------------------------------------
