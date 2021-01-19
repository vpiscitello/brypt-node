//------------------------------------------------------------------------------------------------
// File: PeerObserver.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Network/ConnectionState.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

class BryptPeer;

//------------------------------------------------------------------------------------------------

class IPeerObserver
{
public:
    virtual ~IPeerObserver() = default;

    virtual void HandlePeerStateChange(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        ConnectionState change) = 0;
};

//------------------------------------------------------------------------------------------------