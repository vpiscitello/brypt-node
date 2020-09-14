//------------------------------------------------------------------------------------------------
// File: BootstrapCache.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Utilities/CallbackIteration.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
//------------------------------------------------------------------------------------------------

namespace BryptIdentifier {
  class CContainer;
  using SharedContainer = std::shared_ptr<CContainer>;
}

//------------------------------------------------------------------------------------------------

class IPeerCache
{
public:
    virtual ~IPeerCache() = default;

    using IdentifierReadFunction = std::function<
        CallbackIteration(BryptIdentifier::SharedContainer const& spBryptIdentifier)>;

    virtual bool ForEachCachedIdentifier(IdentifierReadFunction const& readFunction) const = 0;

    virtual std::uint32_t ActivePeerCount() const = 0;

};

//------------------------------------------------------------------------------------------------