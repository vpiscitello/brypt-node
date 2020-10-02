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
    enum class Phase { Flood, Respond, Aggregate, Close };
    
    explicit CQueryHandler(CBryptNode& instance);

    // IHandler{
    bool HandleMessage(AssociatedMessage const& associatedMessage) override;
    // }IHandler

    bool FloodHandler(std::weak_ptr<CBryptPeer> const& wpBryptPeer, CApplicationMessage const& message);
    bool RespondHandler(std::weak_ptr<CBryptPeer> const& wpBryptPeer, CApplicationMessage const& message);
    bool AggregateHandler(std::weak_ptr<CBryptPeer> const& wpBryptPeer, CApplicationMessage const& message);
    bool CloseHandler();

};

//------------------------------------------------------------------------------------------------
