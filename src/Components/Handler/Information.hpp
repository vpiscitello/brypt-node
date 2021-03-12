//----------------------------------------------------------------------------------------------------------------------
// File: Information.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Handles the flood phase for the Information type handler
//----------------------------------------------------------------------------------------------------------------------
class Handler::Information : public Handler::IHandler
{
public:
    enum class Phase : std::uint8_t { Flood, Respond, Close };

    explicit Information(BryptNode& instance);

    // IHandler{
    bool HandleMessage(AssociatedMessage const& associatedMessage) override;
    // }IHandler

    bool FloodHandler(
        std::weak_ptr<BryptPeer> const& wpBryptPeer, ApplicationMessage const& message);
    bool RespondHandler();
    bool CloseHandler();
};

//----------------------------------------------------------------------------------------------------------------------
