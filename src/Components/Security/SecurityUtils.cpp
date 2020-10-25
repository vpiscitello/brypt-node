//------------------------------------------------------------------------------------------------
// File: SecurityUtils.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "SecurityUtils.hpp"
//------------------------------------------------------------------------------------------------
#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif
//------------------------------------------------------------------------------------------------
#include <cstring>
//------------------------------------------------------------------------------------------------

Security::Strategy Security::ConvertToStrategy(std::underlying_type_t<Security::Strategy> strategy)
{
    using StrategyType = std::underlying_type_t<Security::Strategy>;

    switch (strategy) {
        case static_cast<StrategyType>(Security::Strategy::PQNISTL3): {
            return Security::Strategy::PQNISTL3;
        }
        default: break;
    }
    return Security::Strategy::Invalid;
}

//------------------------------------------------------------------------------------------------

void Security::EraseMemory(void* begin, std::uint32_t size)
{
#if defined(__STDC_LIB_EXT1__)
    std::memset_s(begin, size, 0, size);
#else
    auto data = reinterpret_cast<std::uint8_t volatile*>(begin);
    for (std::uint32_t erased = 0; erased < size; ++erased) {
        data[erased] = 0x00;
    }
#endif
}

//------------------------------------------------------------------------------------------------
