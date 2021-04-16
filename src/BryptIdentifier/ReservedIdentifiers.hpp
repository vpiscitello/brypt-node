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
bool IsIdentifierReserved(Internal::Identifier::Type const& identifier);
bool IsIdentifierReserved(std::string_view identifier);
bool IsIdentifierReserved(Node::Identifier const& identifier);

bool IsIdentifierAllowed(Internal::Identifier::Type const& identifier);
bool IsIdentifierAllowed(std::string_view identifier);
bool IsIdentifierAllowed(Node::Identifier const& identifier);

//----------------------------------------------------------------------------------------------------------------------
namespace Internal::Identifier {
//----------------------------------------------------------------------------------------------------------------------

// Indicates an invalid node id that is not addressable/reachable
constexpr Node::Internal::Identifier::Type const Invalid = std::numeric_limits<Type>::min();

//----------------------------------------------------------------------------------------------------------------------
} // Internal namespace
//----------------------------------------------------------------------------------------------------------------------
namespace Network::Identifier {
//----------------------------------------------------------------------------------------------------------------------

// NOTE: The following the are the pre-computed network representations of the above reserved in the application's
// internal representation. 
constexpr std::string_view const Invalid = "bry0:11111111111111114fq2it";

//----------------------------------------------------------------------------------------------------------------------
} // Network namespace
} // ReservedIdentifiers namespace
//----------------------------------------------------------------------------------------------------------------------
