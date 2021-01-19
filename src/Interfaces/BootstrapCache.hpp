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

class BryptPeer;

//------------------------------------------------------------------------------------------------

class IBootstrapCache
{
public:
    virtual ~IBootstrapCache() = default;

    using AllProtocolsReadFunction = std::function<
        CallbackIteration(Network::Protocol, std::string_view const& bootstrap)>;
    using AllProtocolsErrorFunction = std::function<void(Network::Protocol)>;
    using OneProtocolReadFunction = std::function<
        CallbackIteration(std::string_view const& bootstrap)>;

    virtual bool ForEachCachedBootstrap(
        AllProtocolsReadFunction const& callback,
        AllProtocolsErrorFunction const& error) const = 0;
    virtual bool ForEachCachedBootstrap(
        Network::Protocol protocol,
        OneProtocolReadFunction const& callback) const = 0;

    virtual std::size_t CachedBootstrapCount() const = 0;
    virtual std::size_t CachedBootstrapCount(Network::Protocol protocol) const = 0;
};

//------------------------------------------------------------------------------------------------