//------------------------------------------------------------------------------------------------
// File: MessageSecurity.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <optional>
#include <string_view>
//------------------------------------------------------------------------------------------------

class CApplicationMessage;

//------------------------------------------------------------------------------------------------
namespace MessageSecurity {
//------------------------------------------------------------------------------------------------

enum class VerificationStatus : std::uint8_t { Unauthorized, Success };

constexpr std::uint32_t TokenSize = 32;
constexpr std::string_view HashMethod = "blake2s256";

std::optional<Message::Buffer> Encrypt(
    Message::Buffer const& buffer, std::uint32_t length, std::uint64_t nonce);
std::optional<Message::Buffer> Decrypt(
    Message::Buffer const& buffer, std::uint32_t length, std::uint64_t nonce);
std::optional<Message::Buffer> HMAC(Message::Buffer const& buffer, std::uint32_t length);

VerificationStatus Verify(Message::Buffer const& buffer);
	
//------------------------------------------------------------------------------------------------
} // MessageSecurity namespace
//------------------------------------------------------------------------------------------------
