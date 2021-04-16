//----------------------------------------------------------------------------------------------------------------------
// File: BootstrapCache.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/CallbackIteration.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
//----------------------------------------------------------------------------------------------------------------------

class IPeerCache
{
public:
    virtual ~IPeerCache() = default;

    enum class Filter : std::uint8_t { None, Active, Inactive };

    using IdentifierReadFunction = std::function<CallbackIteration(Node::SharedIdentifier const& spNodeIdentifier)>;

    virtual bool ForEachCachedIdentifier(
      IdentifierReadFunction const& callback, Filter filter = Filter::Active) const = 0;

    virtual std::size_t ActivePeerCount() const = 0;
    virtual std::size_t InactivePeerCount() const = 0;
    virtual std::size_t ObservedPeerCount() const = 0;

    virtual std::size_t ResolvingPeerCount() const = 0;
};

//----------------------------------------------------------------------------------------------------------------------