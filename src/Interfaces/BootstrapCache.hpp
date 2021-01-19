//------------------------------------------------------------------------------------------------
// File: BootstrapCache.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Network/Protocol.hpp"
#include "Utilities/CallbackIteration.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>
//------------------------------------------------------------------------------------------------

class CBryptPeer;

//------------------------------------------------------------------------------------------------

class IBootstrapCache
{
public:
    virtual ~IBootstrapCache() = default;

    using AllEndpointBootstrapReadFunction = std::function<
        CallbackIteration(Endpoints::TechnologyType, std::string_view const& bootstrap)>;
    using AllEndpointBootstrapErrorFunction = std::function<void(Endpoints::TechnologyType)>;
    using OneEndpointBootstrapReadFunction = std::function<
        CallbackIteration(std::string_view const& bootstrap)>;

    virtual bool ForEachCachedBootstrap(
        AllEndpointBootstrapReadFunction const& callback,
        AllEndpointBootstrapErrorFunction const& error) const = 0;
    virtual bool ForEachCachedBootstrap(
        Endpoints::TechnologyType technology,
        OneEndpointBootstrapReadFunction const& callback) const = 0;

    virtual std::size_t CachedBootstrapCount() const = 0;
    virtual std::size_t CachedBootstrapCount(Endpoints::TechnologyType technology) const = 0;
};

//------------------------------------------------------------------------------------------------