//------------------------------------------------------------------------------------------------
// File: MessageSecurity.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageDefinitions.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <optional>
#include <string_view>
//------------------------------------------------------------------------------------------------

class CMessage;

//------------------------------------------------------------------------------------------------
namespace MessageSecurity {
//------------------------------------------------------------------------------------------------

enum class VerificationStatus : std::uint8_t { Success, Unauthorized };

constexpr std::uint32_t TokenSize = 32;
constexpr std::string_view HashMethod = "blake2s256";

std::optional<Message::Buffer> Encrypt(
    Message::Buffer const& message,
    std::uint32_t length,
    NodeUtils::NetworkNonce nonce);
std::optional<Message::Buffer> Decrypt(
    Message::Buffer const& message,
    std::uint32_t length,
    NodeUtils::NetworkNonce nonce);
std::optional<Message::Buffer> HMAC(Message::Buffer const& message, std::uint32_t length);

VerificationStatus Verify(CMessage const& message);
VerificationStatus Verify(Message::Buffer const& buffer);
VerificationStatus Verify(std::string_view pack);
	
//------------------------------------------------------------------------------------------------
} // MessageSecurity namespace
//------------------------------------------------------------------------------------------------
