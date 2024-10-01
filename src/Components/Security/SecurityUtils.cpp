//----------------------------------------------------------------------------------------------------------------------
// File: SecurityUtils.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "SecurityUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/rand.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <cstring>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Generate and return a buffer of the provided size filled with random data. 
//----------------------------------------------------------------------------------------------------------------------
Security::OptionalBuffer Security::GenerateRandomData(std::size_t size)
{
    assert(std::in_range<std::int32_t>(size));
    auto buffer = std::vector<std::uint8_t>(size, 0x00);
    if (!RAND_bytes(buffer.data(), static_cast<std::int32_t>(size))) { return {}; }
    return buffer;
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::GenerateRandomData(WriteableView writeable)
{
    assert(std::in_range<std::int32_t>(writeable.size()));
    if (!RAND_bytes(writeable.data(), static_cast<std::int32_t>(writeable.size()))) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::EraseMemory(void* begin, std::size_t size)
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

//----------------------------------------------------------------------------------------------------------------------
