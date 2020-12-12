//------------------------------------------------------------------------------------------------
// File: PackUtils.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <bit>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <type_traits>
#include <vector>
//------------------------------------------------------------------------------------------------
#include <boost/endian/conversion.hpp>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace PackUtils {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Note: Classes and Structs with more than one member should not use this method. 
//------------------------------------------------------------------------------------------------
template<typename Source>
	requires std::is_standard_layout_v<Source> && std::is_trivial_v<Source> && 
		(sizeof(Source) > 0 && sizeof(Source) < 9)	
void PackChunk(Source const& source, std::vector<std::uint8_t>& destination)
{
	constexpr std::uint32_t SourceBytes = sizeof(Source);
	if constexpr (std::endian::native == std::endian::little) {
		auto const size = destination.size();
		auto const begin = destination.data() + size;
		destination.resize(size + SourceBytes, 0x00);
		boost::endian::endian_store<Source, SourceBytes, boost::endian::order::big>(
			begin, source);
	} else {
		auto const begin = reinterpret_cast<std::uint8_t const*>(&source);
		auto const end = begin + SourceBytes;
		destination.insert(destination.end(), begin, end);
	}
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Note: Variable length buffers must be preceeded by its size. The caller must supply a unsigned 
// intergral type to indicate the size limit and ensure the source buffer does not exceed the 
// maxmimum value representable by that type. If no type is provided, it assumed the source 
// buffer is of a fixed size buffer and thus no size field will be appended. 
//------------------------------------------------------------------------------------------------
template<typename SizeField = void>
	requires std::unsigned_integral<SizeField> || std::is_void_v<SizeField>
inline void PackChunk(
	std::vector<std::uint8_t> const& source, std::vector<std::uint8_t>& destination)
{
	// If the SizeField type is not void then preceed the buffer with its size.
	if constexpr (!std::is_void_v<SizeField>) {
		constexpr std::uint32_t FieldLimit = sizeof(SizeField);
		assert(static_cast<double>(source.size()) < std::pow(2, 8 * FieldLimit));
		auto const size = static_cast<SizeField>(source.size());
		PackChunk(size, destination);
	}

	destination.insert(destination.end(), source.begin(), source.end());
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Note: Variable length buffers must be preceeded by its size. The caller must supply a unsigned 
// intergral type to indicate the size limit and ensure the source buffer does not exceed the 
// maxmimum value representable by that type. If no type is provided, it assumed the source 
// buffer is of a fixed size buffer and thus no size field will be appended. 
//------------------------------------------------------------------------------------------------
template<typename SizeField = void>
	requires std::unsigned_integral<SizeField> || std::is_void_v<SizeField>
inline void PackChunk(
	std::string_view source, std::vector<std::uint8_t>& destination)
{
	// If the SizeField type is not void then preceed the buffer with its size.
	if constexpr (!std::is_void_v<SizeField>) {
		constexpr std::uint32_t FieldLimit = sizeof(SizeField);
		assert(static_cast<double>(source.size()) < std::pow(2, 8 * FieldLimit));
		auto const size = static_cast<SizeField>(source.size());
		PackChunk(size, destination);
	}

	destination.insert(destination.end(), source.begin(), source.end());
}

//------------------------------------------------------------------------------------------------

template<std::input_iterator Source, typename Destination>
	requires std::is_standard_layout_v<Destination> && std::is_trivial_v<Destination> &&
		std::indirectly_copyable<Source, std::uint8_t*>
bool UnpackChunk(Source& begin, Source const& end, Destination& destination)
{
    constexpr std::uint32_t DestinationBytes = sizeof(destination);

	// If the buffer does not contain enough data to unpack the chunk unpacking cannot occur. 
	if (auto const available = std::distance(begin, end); available < DestinationBytes) {
		return false;
	}

	std::copy_n(begin, DestinationBytes, reinterpret_cast<std::uint8_t*>(&destination));
	if constexpr (std::endian::native == std::endian::little) {
		boost::endian::big_to_native_inplace(destination);
	}

    begin += DestinationBytes;
	return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Note: It is assumed the caller has reserved enough space for the incoming data in the 
// destination buffer. It is that capacity that is used to determine the amount data to copy
// from the source buffer. 
//------------------------------------------------------------------------------------------------
template<std::input_iterator Source>
	requires std::indirectly_copyable<Source, std::vector<std::uint8_t>::iterator> 
bool UnpackChunk(
	Source& begin, Source const& end, std::vector<std::uint8_t>& destination)
{
	std::uint32_t const capacity = destination.capacity();

	// If the buffer does not contain enough data to unpack the chunk unpacking cannot occur. 
	if (auto const available = std::distance(begin, end); available < capacity) {
		return false;
	}

	// Insert the buffer section into the destination
	destination.insert(destination.begin(), begin, begin + capacity);
	assert(destination.size() == capacity);

	begin += capacity;
	return true;
}

//------------------------------------------------------------------------------------------------
} // PackUtils namespace
//------------------------------------------------------------------------------------------------
