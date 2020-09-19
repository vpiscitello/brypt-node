
//------------------------------------------------------------------------------------------------
// File: ReservedIdentifiers.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier.hpp"
#include "ReservedIdentifiers.hpp"
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
//------------------------------------------------------------------------------------------------

bool ReservedIdentifiers::IsIdentifierReserved(BryptIdentifier::BufferType const& buffer)
{
    if (buffer.size() != BryptIdentifier::InternalSize) {
        return true;
    }

    auto const optInternalRepresentation = BryptIdentifier::ConvertToInternalRepresentation(buffer);
    return IsIdentifierReserved(*optInternalRepresentation);
}

//------------------------------------------------------------------------------------------------

bool ReservedIdentifiers::IsIdentifierReserved(BryptIdentifier::InternalType const& identifier)
{
    if (identifier == ReservedIdentifiers::Internal::ClusterRequest) {
        return true;
    }

    if (identifier == ReservedIdentifiers::Internal::NetworkRequest) {
        return true;
    }

    if (identifier == ReservedIdentifiers::Internal::Unknown) {
        return true;
    }

    if (identifier == ReservedIdentifiers::Internal::Invalid) {
        return true;
    }
    
    return false;
}

//------------------------------------------------------------------------------------------------

bool ReservedIdentifiers::IsIdentifierReserved(std::string_view identifier)
{
    if (identifier == ReservedIdentifiers::Network::ClusterRequest) {
        return true;
    }

    if (identifier == ReservedIdentifiers::Network::NetworkRequest) {
        return true;
    }

    if (identifier == ReservedIdentifiers::Network::Unknown) {
        return true;
    }

    if (identifier == ReservedIdentifiers::Network::Invalid) {
        return true;
    }
    
    return false;
}

//------------------------------------------------------------------------------------------------

bool ReservedIdentifiers::IsIdentifierReserved(BryptIdentifier::CContainer const& identifier)
{
    return IsIdentifierReserved(identifier.GetInternalRepresentation());
}

//------------------------------------------------------------------------------------------------

bool ReservedIdentifiers::IsIdentifierAllowed(BryptIdentifier::InternalType const& identifier)
{
    if (identifier == ReservedIdentifiers::Internal::Invalid) {
        return false;
    }
    
    return true;
}

//------------------------------------------------------------------------------------------------

bool ReservedIdentifiers::IsIdentifierAllowed(std::string_view identifier)
{
    if (identifier == ReservedIdentifiers::Network::Invalid) {
        return false;
    }
    
    return true;
}

//------------------------------------------------------------------------------------------------


bool ReservedIdentifiers::IsIdentifierAllowed(BryptIdentifier::CContainer const& identifier)
{
    return IsIdentifierAllowed(identifier.GetInternalRepresentation());
}

//------------------------------------------------------------------------------------------------
