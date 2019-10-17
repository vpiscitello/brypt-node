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
class Command::CQuery : public Command::CHandler {
public:
    CQuery(CNode& instance, std::weak_ptr<CState> const& state);
    ~CQuery() override {};

    void whatami() override;
    bool HandleMessage(CMessage const& message) override;

    bool FloodHandler(CMessage const& message);
    bool RespondHandler(CMessage const& message);
    bool AggregateHandler(CMessage const& message);
    bool CloseHandler();

private:
    enum class Phase { FLOOD, RESPOND, AGGREGATE, CLOSE };
};

//------------------------------------------------------------------------------------------------
