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
class Command::CConnectHandler : public Command::IHandler {
public:
    explicit CConnectHandler(CBryptNode& instance);

    // IHandler{
    bool HandleMessage(CMessage const& message) override;
    // }IHandler

    bool ContactHandler();
    bool JoinHandler();
    bool CloseHandler();

private:
    enum class Phase { Contact, Join, Close };
};

//------------------------------------------------------------------------------------------------
