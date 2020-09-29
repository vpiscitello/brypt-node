//------------------------------------------------------------------------------------------------
// File: PackUtils.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <zmq.h>
//------------------------------------------------------------------------------------------------
#include <cassert>
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

//------------------------------------------------------------------------------------------------
// Note: Variable length data chunks must be preceeded by the size of the buffer. The caller 
// must ensure the buffer size can be represented by the interger type to contain the size. 
// The limit is the integer type's size in bytes. 
//------------------------------------------------------------------------------------------------
inline void PackChunk(Message::Buffer& buffer, Message::Buffer const& chunk, std::uint32_t limit = 0)
{
	assert(static_cast<double>(chunk.size()) < std::pow(2, 8 * limit));
	switch (limit) {
		// A limit type of 0 assumes that the buffer length is static for the field and the 
		// preceeding size is not needed to decode the data. 
		case 0: break;
		case 1: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 2: {
			auto const size = static_cast<std::uint16_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 4: {
			auto const size = static_cast<std::uint32_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 8: {
			auto const size = static_cast<std::uint64_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		default: assert(false); break;
	}
	buffer.insert(buffer.end(), chunk.begin(), chunk.end());
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Note: Variable length data chunks must be preceeded by the size of the buffer. The caller 
// must ensure the buffer size can be represented by the interger type to contain the size. 
// The limit is the integer type's size in bytes. 
//------------------------------------------------------------------------------------------------
inline void PackChunk(Message::Buffer& buffer, std::string const& chunk, std::uint32_t limit = 0)
{
	assert(static_cast<double>(chunk.size()) < std::pow(2, 8 * limit));
	switch (limit) {
		// A limit type of 0 assumes that the buffer length is static for the field and the 
		// preceeding size is not needed to decode the data. 
		case 0: break;
		case 1: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 2: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 4: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 8: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		default: assert(false); break;
	}
	buffer.insert(buffer.end(), chunk.begin(), chunk.end());
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Note: Variable length data chunks must be preceeded by the size of the buffer. The caller 
// must ensure the buffer size can be represented by the interger type to contain the size. 
// The limit is the integer type's size in bytes. 
//------------------------------------------------------------------------------------------------
inline void PackChunk(Message::Buffer& buffer, std::string_view chunk, std::uint32_t limit = 0)
{
	assert(static_cast<double>(chunk.size()) < std::pow(2, 8 * limit));
	switch (limit) {
		// A limit type of 0 assumes that the buffer length is static for the field and the 
		// preceeding size is not needed to decode the data. 
		case 0: break;
		case 1: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 2: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 4: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		case 8: {
			auto const size = static_cast<std::uint8_t>(chunk.size());
			PackChunk(buffer, size);
		} break;
		default: assert(false); break;
	}
	buffer.insert(buffer.end(), chunk.begin(), chunk.end());
}

//------------------------------------------------------------------------------------------------

template<typename ChunkType>
bool UnpackChunk(
	Message::Buffer::const_iterator& bufferBegin,
	Message::Buffer::const_iterator const& bufferEnd,
	ChunkType& destination)
{
    std::uint32_t const bytesToUnpack = sizeof(destination);

	// If the buffer does not contain enough data to unpack the chunk unpacking cannot occur. 
	if (auto const bytesAvailable = std::distance(bufferBegin, bufferEnd);
		bytesAvailable < bytesToUnpack) {
		return false;
	}

	std::copy_n(bufferBegin, bytesToUnpack, reinterpret_cast<std::uint8_t*>(&destination));
    bufferBegin += bytesToUnpack;

	return true;
}

//------------------------------------------------------------------------------------------------

inline bool UnpackChunk(
	Message::Buffer::const_iterator& bufferBegin,
	Message::Buffer::const_iterator const& bufferEnd,
	Message::Buffer& destination,
	std::uint32_t bytesToUnpack)
{
	Message::Buffer::const_iterator sectionEnd = bufferBegin + bytesToUnpack;

	// If the buffer does not contain enough data to unpack the chunk unpacking cannot occur. 
	if (auto const bytesAvailable = std::distance(bufferBegin, bufferEnd);
		bytesAvailable < bytesToUnpack) {
		return false;
	}

	// Insert the buffer section into the destination
	destination.insert(destination.begin(), bufferBegin, sectionEnd);
	bufferBegin = sectionEnd;

	return true;
}

//------------------------------------------------------------------------------------------------

inline bool UnpackChunk(
	Message::Buffer::const_iterator& bufferBegin,
	Message::Buffer::const_iterator const& bufferEnd,
	Message::Buffer::iterator const& destinationBegin,
	Message::Buffer::iterator const& destinationEnd)
{
	auto const bytesToUnpack = std::distance(destinationBegin, destinationEnd);

	// If the buffer does not contain enough data to unpack the chunk unpacking cannot occur. 
	if (auto const bytesAvailable = std::distance(bufferBegin, bufferEnd);
		bytesAvailable < bytesToUnpack) {
		return false;
	}

	// Copy the entir contents of the buffer into the destination
	std::copy(bufferBegin, bufferEnd, destinationBegin);
	bufferBegin += bytesToUnpack;

	return true;
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
