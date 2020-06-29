//------------------------------------------------------------------------------------------------
// File: PeerMediator.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/ConnectionState.hpp"
//------------------------------------------------------------------------------------------------

class CPeer;
class IPeerObserver;

//------------------------------------------------------------------------------------------------

class IPeerMediator
{
public:
    virtual ~IPeerMediator() = default;

    virtual void RegisterObserver(IPeerObserver* const observer) = 0;
    virtual void UnpublishObserver(IPeerObserver* const observer) = 0;

    virtual void ForwardPeerConnectionStateChange(CPeer const& peer, ConnectionState change) = 0;
};

//------------------------------------------------------------------------------------------------