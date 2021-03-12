//----------------------------------------------------------------------------------------------------------------------
// File: StatusCode.hpp
// Description: Holds enumeration values for configuration parsing
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Configuration {
//----------------------------------------------------------------------------------------------------------------------

enum class StatusCode : std::uint8_t { Success, DecodeError, InputError, FileError };

//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------
