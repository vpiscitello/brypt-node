
//----------------------------------------------------------------------------------------------------------------------
// File: ReservedIdentifiers.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier.hpp"
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
    if (buffer.size() != Identifier::PayloadBytes) { return true; }
    auto const optInternalRepresentation = Node::ToInternalIdentifier(buffer);
    return IsIdentifierReserved(*optInternalRepresentation);
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierReserved(Internal::Identifier const& identifier)
{
    if (identifier == Internal::InvalidIdentifier) { return true; }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierReserved(std::string_view identifier)
{
    if (identifier == External::InvalidIdentifier) { return true; }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierReserved(Node::Identifier const& identifier)
{
    return IsIdentifierReserved(static_cast<Node::Internal::Identifier const&>(identifier));
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierAllowed(Node::Internal::Identifier const& identifier)
{
    if (identifier == Internal::InvalidIdentifier) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::IsIdentifierAllowed(std::string_view identifier)
{
    if (identifier == External::InvalidIdentifier) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------


bool Node::IsIdentifierAllowed(Node::Identifier const& identifier)
{
    return IsIdentifierAllowed(static_cast<Node::Internal::Identifier const&>(identifier));
}

//----------------------------------------------------------------------------------------------------------------------
