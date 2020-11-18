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
    bool HandleMessage(AssociatedMessage const& associatedMessage) override;
    // }IHandler

    bool DiscoveryHandler(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer, CApplicationMessage const& message);
        
    bool JoinHandler(CApplicationMessage const& message);    
};

//------------------------------------------------------------------------------------------------
