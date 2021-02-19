//------------------------------------------------------------------------------------------------
// File: Z85.hpp
// Description: Base85 encoding utilities using the ZeroMQ variant of ASCII85. The following code
// has been adapted from libzmq's implementation. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Z85 {
//------------------------------------------------------------------------------------------------

using ReadableView = std::span<std::uint8_t const, std::dynamic_extent>;
using WritableView = std::span<std::uint8_t, std::dynamic_extent>;

constexpr double Multiplier = 1.25;
constexpr std::uint32_t CharacterSpace = 85;
constexpr std::uint8_t CharacterOffset = 32;
constexpr std::uint32_t EncodedBlockSize = 5;
constexpr std::uint32_t DecodedBlockSize = 4;
constexpr std::uint32_t EncodeDivisor = 85 * 85 * 85 * 85;
constexpr std::uint32_t DecodeDivisor = 256 * 256 * 256;

constexpr std::array<std::uint8_t, CharacterSpace> EncodeMapping = {
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 
    'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
    'u', 'v', 'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
    'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
    'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', '.', '-', ':', '+', '=', '^', '!', '/',
    '*', '?', '&', '<', '>', '(', ')', '[', ']', '{',
    '}', '@', '%', '$', '#'
};

constexpr std::array<std::uint8_t, 96> DecodeMapping = {
    0xFF, 0x44, 0xFF, 0x54, 0x53, 0x52, 0x48, 0xFF,
    0x4B, 0x4C, 0x46, 0x41, 0xFF, 0x3F, 0x3E, 0x45,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x40, 0xFF, 0x49, 0x42, 0x4A, 0x47,
    0x51, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
    0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x3B, 0x3C, 0x3D, 0x4D, 0xFF, 0x4E, 0x43, 0xFF,
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
    0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
    0x21, 0x22, 0x23, 0x4F, 0xFF, 0x50, 0xFF, 0xFF };

[[nodiscard]] constexpr std::size_t EncodedSize(std::size_t size);
[[nodiscard]] constexpr std::size_t DecodedSize(std::size_t size);
[[nodiscard]] constexpr std::size_t PaddingBytes(std::size_t size);

[[nodiscard]] bool Encode(ReadableView source, WritableView destination);
[[nodiscard]] std::string Encode(ReadableView source);

[[nodiscard]] bool Decode(std::string_view source, WritableView destination);
[[nodiscard]] bool Decode(ReadableView source, WritableView destination);
[[nodiscard]] std::vector<std::uint8_t> Decode(std::string_view source);
[[nodiscard]] std::vector<std::uint8_t> Decode(ReadableView source);

//------------------------------------------------------------------------------------------------
} // Base58 namespace
//------------------------------------------------------------------------------------------------

inline constexpr std::size_t Z85::EncodedSize(std::size_t size)
{
    size += PaddingBytes(size); 
    return size * EncodedBlockSize / DecodedBlockSize;
}

//------------------------------------------------------------------------------------------------

inline constexpr std::size_t Z85::DecodedSize(std::size_t size)
{
    return size * DecodedBlockSize / EncodedBlockSize;
}

//------------------------------------------------------------------------------------------------

inline constexpr std::size_t Z85::PaddingBytes(std::size_t size)
{
    return (4 - (size & 3)) & 3;
}

//------------------------------------------------------------------------------------------------

inline bool Z85::Encode(ReadableView source, WritableView destination)
{
    std::uint32_t index = 0;
    std::uint32_t value = 0;
    std::size_t size = source.size();
    std::size_t padding = PaddingBytes(size);
    auto writable = destination.data();

    if (destination.size() != EncodedSize(source.size())) { return false; }

    // Helper to handle encoding bytes into Base256. In addition to encoding the value,
    // bytes read counter will be incremented. 
    #define EncodeBase256(index, value, byte)\
        ++index; value = (value * 256) + byte;

    // Helper to handle encoding Base256 values into Z85. In addition to encoding the value, 
    // the writable pointer will be incremented and reset the value.
    #define EncodeZ85(writable, value) \
        for (auto divisor = EncodeDivisor; divisor; divisor /= CharacterSpace) {\
        *(writable++) = EncodeMapping[value / divisor % CharacterSpace]; } value = 0;

    // Apply the encoding to the source buffer. 
    for (auto const& decoded : source) {
        // Encode the byte into the value using Base256.
        EncodeBase256(index, value, decoded)
        // If the block has been fulfilled encode the bytes with Z85
        if (index % DecodedBlockSize == 0) { EncodeZ85(writable, value) }
    }

    // If there is left over data, pad the value until we have a valid block and encode it. 
    if (index % DecodedBlockSize != 0) {
        for (std::uint32_t idx = 0; idx < padding; ++idx) { EncodeBase256(index, value, 0x00) }
        EncodeZ85(writable, value)
        assert(index % DecodedBlockSize == 0);
    }

    // It is expected that all of the source buffer has been read 
    assert(index == size + padding);
    // It is expected all of the encoded data has been written and that the past-the-end value 
    // has not been modified. 
    assert(writable == (destination.data() + destination.size()) && *writable == std::uint8_t(0x00));

    return true;
}

//------------------------------------------------------------------------------------------------

inline std::string Z85::Encode(ReadableView source)
{
    std::string destination(EncodedSize(source.size()), '\0');
    WritableView writable(reinterpret_cast<std::uint8_t*>(destination.data()), destination.size());
    if (!Encode(source, writable)) { return {}; }
    return destination;
}

//------------------------------------------------------------------------------------------------

inline bool Z85::Decode(std::string_view source, WritableView destination)
{
    std::uint32_t index = 0;
    std::uint32_t value = 0;
    std::size_t size = source.size();
    auto writable = destination.data();

    assert(destination.size() == DecodedSize(size));
    assert(destination.size() % DecodedBlockSize == 0);
    assert(size >= EncodedBlockSize && size % EncodedBlockSize == 0);

    if (size < EncodedBlockSize || size % EncodedBlockSize != 0) { return false; }

    // Helper to handle decoding Z85 values into Base256. In addition to encoding the value, 
    // the writable pointer will be incremented and reset the value.
    #define DecodeBase256(writable, value) \
        for (auto divisor = DecodeDivisor; divisor; divisor /= 256) {\
        *(writable++) = value / divisor % 256; } value = 0;

    // Decoding constants.
    constexpr auto DecodedBlockMaximum = std::numeric_limits<std::uint32_t>::max();
    constexpr auto ValueUpperBound = DecodedBlockMaximum / CharacterSpace;
    constexpr auto IndexUpperBound = DecodeMapping.size();

    // Apply the decoding to each byte in the encoded source.
    for (auto const& encoded : source) {
        value *= CharacterSpace;

        std::uint8_t const mapping = static_cast<std::uint8_t>(encoded - CharacterOffset);
        if (mapping >= IndexUpperBound) { return false; };

        std::uint32_t const DecodedUpperBound = DecodedBlockMaximum - value;
        std::uint32_t const decoded = DecodeMapping[mapping];
        if (decoded == 0xFF || decoded > DecodedUpperBound) { return false; }
        
        value += decoded;

        if (++index % EncodedBlockSize == 0) { DecodeBase256(writable, value) }
        if (value >= ValueUpperBound) { return false; }
    }

    if (index % EncodedBlockSize != 0) { return false; }

    assert(index == size);

    return true;
}

//------------------------------------------------------------------------------------------------

inline bool Z85::Decode(ReadableView source, WritableView destination)
{
    return Decode({ reinterpret_cast<char const*>(source.data()), source.size() }, destination);
}

//------------------------------------------------------------------------------------------------

inline std::vector<std::uint8_t> Z85::Decode(std::string_view source)
{
    std::vector<std::uint8_t> destination(DecodedSize(source.size()), 0x00);
    if (!Decode(source, destination)) { return {}; }
    return destination;
}

//------------------------------------------------------------------------------------------------

inline std::vector<std::uint8_t> Z85::Decode(ReadableView source)
{
    std::vector<std::uint8_t> destination(DecodedSize(source.size()), 0x00);
    if (!Decode({ reinterpret_cast<char const*>(source.data()), source.size() }, destination)){
        return {};
    }
    return destination;
}

//------------------------------------------------------------------------------------------------
