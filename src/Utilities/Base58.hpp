//------------------------------------------------------------------------------------------------
// File: Base58.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Base58 {
//------------------------------------------------------------------------------------------------

constexpr std::uint32_t CharacterSpace = 58;

constexpr std::array<std::uint8_t, CharacterSpace> EncodeMapping = {
  '1', '2', '3', '4', '5', '6', '7', '8', '9',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J',
  'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T',
  'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 
  'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'm',
  'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z'
};

constexpr std::array<std::uint8_t, 128> DecodeMapping = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
  0x07, 0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0xff, 0x11, 0x12, 0x13, 0x14, 0x15, 0xff,
  0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d,
  0x1e, 0x1f, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
  0x28, 0x29, 0x2a, 0x2b, 0xff, 0x2c, 0x2d, 0x2e,
  0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
  0x37, 0x38, 0x39, 0xff, 0xff, 0xff, 0xff, 0xff
};

[[nodiscard]] constexpr std::uint32_t ResultSize(std::uint32_t size);

void Encode(std::vector<std::uint8_t> const& source, std::string& destination);
[[nodiscard]] std::vector<std::uint8_t> Decode(std::string_view source);

//------------------------------------------------------------------------------------------------
} // Base58 namespace
//------------------------------------------------------------------------------------------------

inline constexpr std::uint32_t Base58::ResultSize(std::uint32_t size)
{
    return ((size * 138) / 100) + 1;
}

//------------------------------------------------------------------------------------------------

inline void Base58::Encode(std::vector<std::uint8_t> const& source, std::string& destination)
{
    std::uint32_t const size = ResultSize(source.size());
    std::vector<std::uint8_t> indicies(size, 0x00);
    std::uint32_t length = 1;

    for (auto const& decoded : source) {
        std::uint32_t carry = static_cast<std::uint32_t>(decoded);
        for (std::uint32_t idx = 0; idx < length; ++idx) {
            carry = carry + static_cast<std::uint32_t>(indicies[idx] << 8);
            indicies[idx] = static_cast<std::uint8_t>(carry % CharacterSpace);
            carry /= CharacterSpace;
        }

        while (carry) {
            indicies[length++] = static_cast<std::uint8_t>(carry % CharacterSpace);
            carry /= CharacterSpace;
        }
    }

    destination.reserve(length);
    for (std::uint32_t idx = 0; idx < (source.size() - 1) && !source[idx]; ++idx) {
        destination.push_back(EncodeMapping[0]);
    }

    for (std::uint32_t idx = 0; idx < length; ++idx) {
        destination.push_back(EncodeMapping[indicies[length - idx - 1]]);
    }
}

//------------------------------------------------------------------------------------------------

inline std::vector<std::uint8_t> Base58::Decode(std::string_view source)
{
    std::uint32_t const size = ResultSize(source.size());
    std::vector<std::uint8_t> decoded(size, 0x00);
    std::uint32_t length = 1;


    for (auto const& encoded : source) {
        std::uint32_t carry = static_cast<std::uint32_t>(DecodeMapping[encoded & 0x7f]);
        for (std::uint32_t idx = 0; idx < length; ++idx) {
            carry += static_cast<std::uint32_t>(decoded[idx] * CharacterSpace);
            decoded[idx] = static_cast<std::uint8_t>(carry);
            carry >>= 8;
        }

        while (carry) {
            decoded[length++] = static_cast<std::uint8_t>(carry);
            carry >>= 8;
        }
    }

    decoded.resize(length);

    for (std::uint32_t idx = 0; idx < (source.size() - 1) && source[idx] == EncodeMapping[0]; ++idx) {
        decoded.push_back(0);
    }

    std::reverse(decoded.begin(), decoded.end());

    return decoded;
}

//------------------------------------------------------------------------------------------------
