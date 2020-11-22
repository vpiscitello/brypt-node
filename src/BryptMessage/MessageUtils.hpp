//------------------------------------------------------------------------------------------------
// File: MessageUtils.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
#include "../BryptIdentifier/IdentifierTypes.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Message {
//------------------------------------------------------------------------------------------------

Message::Protocol ConvertToProtocol(std::underlying_type_t<Message::Protocol> protocol);

std::optional<Message::Protocol> PeekProtocol(
    Message::Buffer::const_iterator const& begin,
    Message::Buffer::const_iterator const& end);

std::optional<std::uint32_t> PeekSize(
    Message::Buffer::const_iterator const& begin,
    Message::Buffer::const_iterator const& end);

std::optional<BryptIdentifier::CContainer> PeekSource(
    Message::Buffer::const_iterator const& begin,
    Message::Buffer::const_iterator const& end);

//------------------------------------------------------------------------------------------------
} // Message namespace
//------------------------------------------------------------------------------------------------