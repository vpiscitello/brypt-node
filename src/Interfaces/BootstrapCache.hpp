//----------------------------------------------------------------------------------------------------------------------
// File: BootstrapCache.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Network/Protocol.hpp"
#include "Utilities/CallbackIteration.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }
namespace Network { class RemoteAddress; }

//----------------------------------------------------------------------------------------------------------------------

class IBootstrapCache
{
public:
    virtual ~IBootstrapCache() = default;

    using AllProtocolsReadFunction = std::function<CallbackIteration(Network::RemoteAddress const& bootstrap)>;
    using AllProtocolsErrorFunction = std::function<void(Network::Protocol)>;
    using OneProtocolReadFunction = std::function<CallbackIteration(Network::RemoteAddress const& bootstrap)>;

    virtual bool ForEachCachedBootstrap(
        AllProtocolsReadFunction const& callback, AllProtocolsErrorFunction const& error) const = 0;
    virtual bool ForEachCachedBootstrap(
        Network::Protocol protocol, OneProtocolReadFunction const& callback) const = 0;

    virtual std::size_t CachedBootstrapCount() const = 0;
    virtual std::size_t CachedBootstrapCount(Network::Protocol protocol) const = 0;
};

//----------------------------------------------------------------------------------------------------------------------