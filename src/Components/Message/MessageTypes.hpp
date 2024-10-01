//----------------------------------------------------------------------------------------------------------------------
// File: MessageTypes.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageDefinitions.hpp"
#include "Components/Awaitable/Definitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <utility>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Message {
//----------------------------------------------------------------------------------------------------------------------

using Version = std::pair<std::uint8_t, std::uint8_t>;
using Buffer = std::vector<std::uint8_t>;

//----------------------------------------------------------------------------------------------------------------------
} // Message namespace
//----------------------------------------------------------------------------------------------------------------------