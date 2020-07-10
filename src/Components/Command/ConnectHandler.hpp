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
    enum class Phase : std::uint8_t { Discovery, Join, };

    explicit CConnectHandler(CBryptNode& instance);

    // IHandler{
    bool HandleMessage(CMessage const& message) override;
    // }IHandler

    bool DiscoveryHandler(CMessage const& message);
    bool JoinHandler(CMessage const& message);    
};

//------------------------------------------------------------------------------------------------
