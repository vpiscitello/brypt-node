//------------------------------------------------------------------------------------------------
// File: MessageUtils.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageDefinitions.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <optional>
#include <span>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Message {
//------------------------------------------------------------------------------------------------

Message::Protocol ConvertToProtocol(std::underlying_type_t<Message::Protocol> protocol);

std::optional<Message::Protocol> PeekProtocol(std::span<std::uint8_t const> buffer);
std::optional<std::uint32_t> PeekSize(std::span<std::uint8_t const> buffer);
std::optional<BryptIdentifier::CContainer> PeekSource(std::span<std::uint8_t const> buffer);

//------------------------------------------------------------------------------------------------
} // Message namespace
//------------------------------------------------------------------------------------------------