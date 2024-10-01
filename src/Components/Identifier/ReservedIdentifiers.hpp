//----------------------------------------------------------------------------------------------------------------------
// File: ReservedIdentifiers.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier.hpp"
#include "IdentifierTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <limits>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Node {
//----------------------------------------------------------------------------------------------------------------------

bool IsIdentifierReserved(std::vector<std::uint8_t> const& buffer);
bool IsIdentifierReserved(Internal::Identifier const& identifier);
bool IsIdentifierReserved(std::string_view identifier);
bool IsIdentifierReserved(Node::Identifier const& identifier);

bool IsIdentifierAllowed(Internal::Identifier const& identifier);
bool IsIdentifierAllowed(std::string_view identifier);
bool IsIdentifierAllowed(Node::Identifier const& identifier);

//----------------------------------------------------------------------------------------------------------------------
namespace Internal {
//----------------------------------------------------------------------------------------------------------------------

// Indicates an invalid node id that is not addressable/reachable
constexpr auto const InvalidIdentifier = std::numeric_limits<Internal::Identifier>::min();

//----------------------------------------------------------------------------------------------------------------------
} // Internal namespace
//----------------------------------------------------------------------------------------------------------------------
namespace External {
//----------------------------------------------------------------------------------------------------------------------

// NOTE: The following the are the pre-computed network representations of the above reserved in the application's
// internal representation. 
constexpr std::string_view const InvalidIdentifier = "bry0:11111111111111114fq2it";

//----------------------------------------------------------------------------------------------------------------------
} // External namespace
} // ReservedIdentifiers namespace
//----------------------------------------------------------------------------------------------------------------------
