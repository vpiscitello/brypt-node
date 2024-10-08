//----------------------------------------------------------------------------------------------------------------------
// Description: C++20 language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

enum class protocol : std::int32_t {
    unknown = BRYPT_UNKNOWN,
    tcp = BRYPT_PROTOCOL_TCP
};

//----------------------------------------------------------------------------------------------------------------------

inline std::string_view to_string(protocol _protocol)
{
	switch (_protocol) {
		case brypt::protocol::unknown: return "unknown";
		case brypt::protocol::tcp: return "tcp";
		default: return "";
	}
}

//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------
