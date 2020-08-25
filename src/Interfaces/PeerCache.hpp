//------------------------------------------------------------------------------------------------
// File: PeerCache.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Utilities/CallbackIteration.hpp"
//------------------------------------------------------------------------------------------------
#include <functional>
#include <utility>
//------------------------------------------------------------------------------------------------

class CPeer;

//------------------------------------------------------------------------------------------------

class IPeerCache
{
public:
    virtual ~IPeerCache() = default;

    using AllEndpointReadFunction = std::function<CallbackIteration(Endpoints::TechnologyType)>;
    using OneEndpointPeersReadFunction = std::function<CallbackIteration(CPeer const&)>;
    using AllEndpointPeersReadFunction = std::function<CallbackIteration(Endpoints::TechnologyType, CPeer const&)>;
    using AllEndpointPeersErrorFunction = std::function<void(Endpoints::TechnologyType)>;

    virtual bool ForEachCachedEndpoint(AllEndpointReadFunction const& readFunction) const = 0;
    virtual bool ForEachCachedPeer(
        AllEndpointPeersReadFunction const& readFunction,
        AllEndpointPeersErrorFunction const& errorFunction) const = 0;
    virtual bool ForEachCachedPeer(
        Endpoints::TechnologyType technology,
        OneEndpointPeersReadFunction const& readFunction) const = 0;

    virtual std::uint32_t CachedEndpointsCount() const = 0;
    virtual std::uint32_t CachedPeersCount() const = 0;
    virtual std::uint32_t CachedPeersCount(Endpoints::TechnologyType technology) const = 0;
};

//------------------------------------------------------------------------------------------------