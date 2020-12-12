//------------------------------------------------------------------------------------------------
// File: ReservedIdentifiers.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier.hpp"
#include "IdentifierTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <limits>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace ReservedIdentifiers {
//------------------------------------------------------------------------------------------------

bool IsIdentifierReserved(std::vector<std::uint8_t> const& buffer);
bool IsIdentifierReserved(BryptIdentifier::Internal::Type const& identifier);
bool IsIdentifierReserved(std::string_view identifier);
bool IsIdentifierReserved(BryptIdentifier::CContainer const& identifier);

bool IsIdentifierAllowed(BryptIdentifier::Internal::Type const& identifier);
bool IsIdentifierAllowed(std::string_view identifier);
bool IsIdentifierAllowed(BryptIdentifier::CContainer const& identifier);

//------------------------------------------------------------------------------------------------
namespace Internal {
//------------------------------------------------------------------------------------------------

// Indicates an invalid node id that is not addressable/reachable
constexpr BryptIdentifier::Internal::Type const Invalid = 
    std::numeric_limits<BryptIdentifier::Internal::Type>::min();

//------------------------------------------------------------------------------------------------
} // Internal namespace
//------------------------------------------------------------------------------------------------
namespace Network {
//------------------------------------------------------------------------------------------------

// NOTE: The following the are the pre-computed network representations of the above reserved 
// in the application's internal representation. 

constexpr std::string_view const Invalid = "bry0:11111111111111114fq2it";

//------------------------------------------------------------------------------------------------
} // Network namespace
} // ReservedIdentifiers namespace
//------------------------------------------------------------------------------------------------
