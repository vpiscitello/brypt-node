//------------------------------------------------------------------------------------------------
// File: PeerObserver.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/ConnectionState.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

class CBryptPeer;

//------------------------------------------------------------------------------------------------

class IPeerObserver
{
public:
    virtual ~IPeerObserver() = default;

    virtual void HandlePeerStateChange(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        ConnectionState change) = 0;
};

//------------------------------------------------------------------------------------------------