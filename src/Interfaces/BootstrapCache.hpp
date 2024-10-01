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
    using BootstrapReader = std::function<CallbackIteration(Network::RemoteAddress const& bootstrap)>;

    virtual ~IBootstrapCache() = default;

    [[nodiscard]] virtual bool Contains(Network::RemoteAddress const& address) const = 0;

    virtual std::size_t ForEachBootstrap(BootstrapReader const& reader) const = 0;
    virtual std::size_t ForEachBootstrap(Network::Protocol protocol, BootstrapReader const& reader) const = 0;

    [[nodiscard]] virtual std::size_t BootstrapCount() const = 0;
    [[nodiscard]] virtual std::size_t BootstrapCount(Network::Protocol protocol) const = 0;
};

//----------------------------------------------------------------------------------------------------------------------