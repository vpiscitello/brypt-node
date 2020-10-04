//------------------------------------------------------------------------------------------------
// File: PeerMediator.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/EndpointIdentifier.hpp"
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

    virtual std::shared_ptr<CBryptPeer> LinkPeer(
        BryptIdentifier::CContainer const& identifier) = 0;

    virtual void DispatchPeerStateChange(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        Endpoints::EndpointIdType identifier,
        Endpoints::TechnologyType technology,
        ConnectionState change) = 0;
};

//------------------------------------------------------------------------------------------------