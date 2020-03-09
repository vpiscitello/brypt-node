//------------------------------------------------------------------------------------------------
// File: InformationHandler.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the flood phase for the Information type command
//------------------------------------------------------------------------------------------------
class Command::CInformationHandler : public Command::IHandler {
public:
    explicit CInformationHandler(CBryptNode& instance);

    // IHandler{
    bool HandleMessage(CMessage const& message) override;
    // }IHandler

    bool FloodHandler(CMessage const& message);
    bool RespondHandler();
    bool CloseHandler();

private:
    enum class Phase { Flood, Respond, Close };
};

//------------------------------------------------------------------------------------------------
