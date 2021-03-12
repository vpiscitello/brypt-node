//----------------------------------------------------------------------------------------------------------------------
// File: SecurityState.hpp
// Description: Enum class for the security states of peers
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

enum class State : std::uint8_t { Unauthorized, Authorized, Flagged };

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------