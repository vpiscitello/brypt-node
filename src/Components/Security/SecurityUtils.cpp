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

void Security::EraseMemory(void* const begin, std::uint32_t size)
{
#if defined(__STDC_LIB_EXT1__)
    std::memset_s(begin, size, 0, size);
#else
    auto volatile data = reinterpret_cast<std::uint8_t volatile* volatile>(begin);
    for (std::uint32_t erased = 0; erased < size; ++erased) {
        data[erased] = 0x00;
    }
#endif
}

//------------------------------------------------------------------------------------------------
