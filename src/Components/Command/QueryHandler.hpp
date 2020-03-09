//------------------------------------------------------------------------------------------------
// File: QueryHandler.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Sensor readings
//------------------------------------------------------------------------------------------------
class Command::CQueryHandler : public Command::IHandler {
public:
    explicit CQueryHandler(CBryptNode& instance);

    // IHandler{
    bool HandleMessage(CMessage const& message) override;
    // }IHandler

    bool FloodHandler(CMessage const& message);
    bool RespondHandler(CMessage const& message);
    bool AggregateHandler(CMessage const& message);
    bool CloseHandler();

private:
    enum class Phase { Flood, Respond, Aggregate, Close };
};

//------------------------------------------------------------------------------------------------
