//------------------------------------------------------------------------------------------------
// File: Query.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Sensor readings
//------------------------------------------------------------------------------------------------
class Handler::Query : public Handler::IHandler
{
public:
    enum class Phase : std::uint8_t { Flood, Respond, Aggregate, Close };
    
    explicit Query(BryptNode& instance);

    // IHandler{
    bool HandleMessage(AssociatedMessage const& associatedMessage) override;
    // }IHandler

    bool FloodHandler(
        std::weak_ptr<BryptPeer> const& wpBryptPeer, ApplicationMessage const& message);
    bool RespondHandler(
        std::weak_ptr<BryptPeer> const& wpBryptPeer, ApplicationMessage const& message);
    bool AggregateHandler(
        std::weak_ptr<BryptPeer> const& wpBryptPeer, ApplicationMessage const& message);
    bool CloseHandler();
};

//------------------------------------------------------------------------------------------------
