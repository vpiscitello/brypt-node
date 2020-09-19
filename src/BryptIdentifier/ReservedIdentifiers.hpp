//------------------------------------------------------------------------------------------------
// File: ReservedIdentifiers.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier.hpp"
//------------------------------------------------------------------------------------------------
#include <limits>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace ReservedIdentifiers {
//------------------------------------------------------------------------------------------------

bool IsIdentifierReserved(BryptIdentifier::BufferType const& buffer);
bool IsIdentifierReserved(BryptIdentifier::InternalType const& identifier);
bool IsIdentifierReserved(std::string_view identifier);
bool IsIdentifierReserved(BryptIdentifier::CContainer const& identifier);

bool IsIdentifierAllowed(BryptIdentifier::InternalType const& identifier);
bool IsIdentifierAllowed(std::string_view identifier);
bool IsIdentifierAllowed(BryptIdentifier::CContainer const& identifier);

//------------------------------------------------------------------------------------------------
namespace Internal {
//------------------------------------------------------------------------------------------------

// Indicates that the destination identifier is unknown. The message should be treated as though
// it is intended for the receiving node. 
constexpr BryptIdentifier::InternalType const Unknown = 0x0;

// Indicates the message is a network-wide request and should be forwarded throughout the entire
// brypt network.
constexpr BryptIdentifier::InternalType const NetworkRequest = 0x1;
// Indicates the message is a cluster-wise request and should be forwarded throughout the current
// cluster. 
constexpr BryptIdentifier::InternalType const ClusterRequest = 0x2;
// Indicates an invalid node id that is not addressable/reachable
constexpr BryptIdentifier::InternalType const Invalid = 
    std::numeric_limits<BryptIdentifier::InternalType>::max();

//------------------------------------------------------------------------------------------------
} // Internal namespace
//------------------------------------------------------------------------------------------------
namespace Network {
//------------------------------------------------------------------------------------------------

// NOTE: The following the are the pre-computed network representations of the above reserved 
// in the application's internal representation. 
constexpr std::string_view const Unknown = "bry0:11111111111111114fq2it";
constexpr std::string_view const NetworkRequest = "bry0:1111111111111118ke19C";
constexpr std::string_view const ClusterRequest = "bry0:111111111111111KmdDu4";
constexpr std::string_view const Invalid = "bry0:4ZrjxJnU1LA5xSyrWMNuXTthCgKt";

//------------------------------------------------------------------------------------------------
} // Network namespace
} // ReservedIdentifiers namespace
//------------------------------------------------------------------------------------------------
