//------------------------------------------------------------------------------------------------
// File: PackUtils.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <zmq.h>
#include <cmath>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace PackUtils {
//------------------------------------------------------------------------------------------------

constexpr static std::double_t Z85Multiplier = 1.25;

template<typename ChunkType>
void PackChunk(Message::Buffer& buffer, ChunkType const& chunk)
{
    auto const begin = reinterpret_cast<std::uint8_t const*>(&chunk);
    auto const end = begin + sizeof(chunk);
    buffer.insert(buffer.end(), begin, end);
}

//------------------------------------------------------------------------------------------------

inline void PackChunk(Message::Buffer& buffer, Message::Buffer const& chunk)
{
	buffer.insert(buffer.end(), chunk.begin(), chunk.end());
}

//------------------------------------------------------------------------------------------------

inline void PackChunk(Message::Buffer& buffer, std::string const& chunk)
{
	buffer.insert(buffer.end(), chunk.begin(), chunk.end());
}

//------------------------------------------------------------------------------------------------

inline void PackChunk(Message::Buffer& buffer, std::string_view chunk)
{
	buffer.insert(buffer.end(), chunk.begin(), chunk.end());
}

//------------------------------------------------------------------------------------------------

template<typename ChunkType>
void UnpackChunk(Message::Buffer const& buffer, std::uint32_t& position, ChunkType& destination)
{
    std::uint32_t size = sizeof(destination);
    std::memcpy(&destination, buffer.data() + position, size);
    position += size;
}

//------------------------------------------------------------------------------------------------

inline void UnpackChunk(
	Message::Buffer const& buffer,
	std::uint32_t& position,
	Message::Buffer& destination,
	std::uint32_t const& size,
	bool bUpdatePositionToEnd = true)
{
	Message::Buffer::const_iterator begin = buffer.begin() + position;
	Message::Buffer::const_iterator end = begin + size;
	destination.insert(destination.begin(), begin, end);
	if (bUpdatePositionToEnd) {
		position += size;
	}
}

//------------------------------------------------------------------------------------------------

inline void UnpackChunk(
	Message::Buffer const& buffer,
	std::uint32_t& position,
	Message::Buffer& destination,
	Message::Buffer::const_iterator const& end,
	bool bUpdatePositionToEnd = true)
{
	auto const beginSize = destination.size();
	Message::Buffer::const_iterator begin = buffer.begin() + position;
	destination.insert(destination.begin(), begin, end);
	auto const endSize = destination.size();

	std::uint32_t const size = static_cast<std::uint32_t>(endSize - beginSize);
	if (bUpdatePositionToEnd) {
		position += size;
	}
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Decode a Z85 message to a std::string
//------------------------------------------------------------------------------------------------
inline Message::Buffer Z85Decode(std::string_view message)
{
    Message::Buffer decoded;
    // Resize the destination to be the estimated maximum size
    decoded.resize(message.size() / Z85Multiplier);
    zmq_z85_decode(decoded.data(), message.data());
    return decoded;
}


//------------------------------------------------------------------------------------------------
// Description: Encode a Message::Buffer to a Z85 message
// Warning: The source buffer may increase in size to be a multiple of 4. Z85 needs to be this 
// padding.
//------------------------------------------------------------------------------------------------
inline std::string Z85Encode(Message::Buffer& message)
{
	std::string encoded;
	// Pad the buffer such that it's length is a multiple of 4 for z85 encoding
	for (std::size_t idx = 0; idx < (message.size() % 4); ++idx) {
		message.emplace_back(0);
	}

	// Resize m_raw to be at least as large as the maximum encoding length of z85
	// z85 will expand the byte length to 25% more than the input length
	encoded.resize(ceil(message.size() * Z85Multiplier));
	zmq_z85_encode(encoded.data(), message.data(), message.size());
	return encoded;
}

//------------------------------------------------------------------------------------------------
} // PackUtils namespace
//------------------------------------------------------------------------------------------------
