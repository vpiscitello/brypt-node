//------------------------------------------------------------------------------------------------
// File: PeerMediator.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Components/Endpoints/ConnectionState.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class IPeerObserver;

//------------------------------------------------------------------------------------------------

class IPeerMediator
{
public:
    virtual ~IPeerMediator() = default;

    virtual void RegisterObserver(IPeerObserver* const observer) = 0;
    virtual void UnpublishObserver(IPeerObserver* const observer) = 0;

    virtual void ForwardConnectionStateChange(
        Endpoints::TechnologyType technology,
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        ConnectionState change) = 0;
};

//------------------------------------------------------------------------------------------------