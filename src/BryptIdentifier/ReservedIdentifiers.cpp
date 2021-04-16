
//----------------------------------------------------------------------------------------------------------------------
// File: ReservedIdentifiers.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier.hpp"
#include "IdentifierDefinitions.hpp"
#include "ReservedIdentifiers.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierReserved(std::vector<std::uint8_t> const& buffer)
{
    if (buffer.size() != Internal::Identifier::PayloadSize) { return true; }
    auto const optInternalRepresentation = Node::ConvertToInternalRepresentation(buffer);
    return IsIdentifierReserved(*optInternalRepresentation);
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierReserved(Internal::Identifier::Type const& identifier)
{
    if (identifier == Node::Internal::Identifier::Invalid) { return true; }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierReserved(std::string_view identifier)
{
    if (identifier == Network::Identifier::Invalid) { return true; }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierReserved(Node::Identifier const& identifier)
{
    return IsIdentifierReserved(identifier.GetInternalValue());
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierAllowed(Node::Internal::Identifier::Type const& identifier)
{
    if (identifier == Internal::Identifier::Invalid) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierAllowed(std::string_view identifier)
{
    if (identifier == Network::Identifier::Invalid) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------


bool Node::IsIdentifierAllowed(Node::Identifier const& identifier)
{
    return IsIdentifierAllowed(identifier.GetInternalValue());
}

//----------------------------------------------------------------------------------------------------------------------
