//----------------------------------------------------------------------------------------------------------------------
// File: PeerObserver.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Network/EndpointIdentifier.hpp"
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------

namespace Network { class RemoteAddress; }

//----------------------------------------------------------------------------------------------------------------------

class IPeerObserver
{
public:
    virtual ~IPeerObserver() = default;
    virtual void OnRemoteConnected(Network::Endpoint::Identifier, Network::RemoteAddress const&) = 0;
    virtual void OnRemoteDisconnected(Network::Endpoint::Identifier, Network::RemoteAddress const&) = 0;
};

//----------------------------------------------------------------------------------------------------------------------