//------------------------------------------------------------------------------------------------
// File: ConnectHandler.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Connecting to a new network or peer
//------------------------------------------------------------------------------------------------
class Command::CConnect : public Command::CHandler {
public:
    CConnect(CNode& instance, std::weak_ptr<CState> const& state);
    ~CConnect() override {};

    void whatami() override;
    bool HandleMessage(CMessage const& message) override;

    bool ContactHandler();
    bool JoinHandler(CMessage const& message);
    bool CloseHandler();
private:
    enum class Phase { CONTACT, JOIN, CLOSE };
};

//------------------------------------------------------------------------------------------------
