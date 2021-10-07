//----------------------------------------------------------------------------------------------------------------------
// File: PeerCache.hpp
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
    enum class Filter : std::uint8_t { None, Active, Inactive };
    using IdentifierReadFunction = std::function<CallbackIteration(Node::SharedIdentifier const& spNodeIdentifier)>;

    virtual ~IPeerCache() = default;
    
    virtual bool ForEach( IdentifierReadFunction const& callback, Filter filter = Filter::Active) const = 0;

    [[nodiscard]] virtual std::size_t ActiveCount() const = 0;
    [[nodiscard]] virtual std::size_t InactiveCount() const = 0;
    [[nodiscard]] virtual std::size_t ObservedCount() const = 0;
    [[nodiscard]] virtual std::size_t ResolvingCount() const = 0;
};

//----------------------------------------------------------------------------------------------------------------------