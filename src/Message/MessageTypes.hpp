//------------------------------------------------------------------------------------------------
// File: Message.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <utility>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Message {
//------------------------------------------------------------------------------------------------

enum class AwaitBinding : std::uint8_t { Source, Destination, None };

using Buffer = std::vector<std::uint8_t>;
using Token = std::vector<std::uint8_t>;
using BoundAwaitingKey = std::pair<AwaitBinding, NodeUtils::ObjectIdType>;

//------------------------------------------------------------------------------------------------
} // Message namespace
//------------------------------------------------------------------------------------------------