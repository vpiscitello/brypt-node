//------------------------------------------------------------------------------------------------
// File: AwaitDefinitions.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Await {
//------------------------------------------------------------------------------------------------

using TrackerKey = std::uint32_t;

enum class UpdateStatus : std::uint8_t { Expired, Unexpected, Success, Fulfilled };
enum class ResponseStatus : std::uint8_t { Unfulfilled, Fulfilled, Completed };

//------------------------------------------------------------------------------------------------
} // Await namespace
//------------------------------------------------------------------------------------------------
