//------------------------------------------------------------------------------------------------
// File: BootstrapCache.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../BryptIdentifier/IdentifierTypes.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Utilities/CallbackIteration.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
//------------------------------------------------------------------------------------------------

class IPeerCache
{
public:
    virtual ~IPeerCache() = default;

    enum class Filter : std::uint8_t { None, Active, Inactive};

    using IdentifierReadFunction = std::function<
        CallbackIteration(BryptIdentifier::SharedContainer const& spBryptIdentifier)>;

    virtual bool ForEachCachedIdentifier(
      IdentifierReadFunction const& callback,
      Filter filter = Filter::Active) const = 0;

    virtual std::uint32_t ActivePeerCount() const = 0;
    virtual std::uint32_t InactivePeerCount() const = 0;
    virtual std::uint32_t ObservedPeerCount() const = 0;

};

//------------------------------------------------------------------------------------------------