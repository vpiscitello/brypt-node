//----------------------------------------------------------------------------------------------------------------------
// File: Query.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Sensor readings
//----------------------------------------------------------------------------------------------------------------------
class Handler::Query : public Handler::IHandler
{
public:
    enum class Phase : std::uint8_t { Flood, Respond, Aggregate, Close };
    
    explicit Query(Node::Core& instance);

    // IHandler{
    bool HandleMessage(AssociatedMessage const& associatedMessage) override;
    // }IHandler

    bool FloodHandler(std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage const& message);
    bool RespondHandler(std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage const& message);
    bool AggregateHandler(std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage const& message);
    bool CloseHandler();
};

//----------------------------------------------------------------------------------------------------------------------
