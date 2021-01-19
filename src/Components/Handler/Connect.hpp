//------------------------------------------------------------------------------------------------
// File: Connect.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Connecting to a new network or peer
//------------------------------------------------------------------------------------------------
class Handler::Connect : public Handler::IHandler {
public:
    enum class Phase : std::uint8_t { Discovery, Join, };

    explicit Connect(BryptNode& instance);

    // IHandler{
    bool HandleMessage(AssociatedMessage const& associatedMessage) override;
    // }IHandler

    bool DiscoveryHandler(
        std::weak_ptr<BryptPeer> const& wpBryptPeer, ApplicationMessage const& message);
        
    bool JoinHandler(ApplicationMessage const& message);    
};

//------------------------------------------------------------------------------------------------
