//----------------------------------------------------------------------------------------------------------------------
// File: BryptIdentifier.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "IdentifierTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/fmt/bundled/format.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Node {
//----------------------------------------------------------------------------------------------------------------------

enum class BufferContentType : std::uint8_t { Internal, Network };

class Identifier;

struct Hasher;

std::string GenerateIdentifier();

std::optional<Internal::Identifier::Type> ConvertToInternalRepresentation(std::vector<std::uint8_t> const& buffer);
std::optional<Internal::Identifier::Type> ConvertToInternalRepresentation(std::string_view identifier);

std::optional<Network::Identifier::Type> ConvertToNetworkRepresentation(Internal::Identifier::Type const& identifier);
std::optional<Network::Identifier::Type> ConvertToNetworkRepresentation(std::vector<std::uint8_t> const& identifier);

std::ostream& operator<<(std::ostream& stream, Node::Identifier const& identifier);
std::stringstream& operator<<(std::stringstream& stream, Node::Identifier const& identifier);
std::stringstream& operator<<(std::stringstream& stream, Node::SharedIdentifier const& spIdentifier);

//----------------------------------------------------------------------------------------------------------------------
} // Node namespace
//----------------------------------------------------------------------------------------------------------------------

class Node::Identifier 
{
public:
    Identifier();
    explicit Identifier(Internal::Identifier::Type const& identifier);
    explicit Identifier(std::string_view identifier);
    explicit Identifier(std::vector<std::uint8_t> const& buffer, BufferContentType type);

    Identifier(Identifier const&) = default;
    Identifier(Identifier&&) = default;
    Identifier& operator=(Identifier const&) = default;
    Identifier& operator=(Identifier&&) = default;

    bool operator<(Identifier const& other) const;
    bool operator==(Identifier const& other) const;
    bool operator!=(Identifier const& other) const;

    friend std::ostream& operator<<(std::ostream& stream, Identifier const& identifier);
    friend std::stringstream& operator<<(std::stringstream& stream, Identifier const& identifier);
    friend std::stringstream& operator<<(std::stringstream& stream, SharedIdentifier const& spIdentifier);

    Internal::Identifier::Type GetInternalValue() const;
    Network::Identifier::Type const& GetNetworkString() const;
    std::size_t NetworkStringSize() const;
    bool IsValid() const;

private:
    void SetupFromInternalRepresentation(Internal::Identifier::Type const& identifier);
    void SetupFromNetworkRepresentation(std::string_view identifier);

    Internal::Identifier::Type m_internal;
    Network::Identifier::Type m_network;
    bool m_valid;
};

//----------------------------------------------------------------------------------------------------------------------

struct Node::Hasher  {
    std::size_t operator() (Identifier const& identifier) const
    {
        return std::hash<Internal::Identifier::Type>()(identifier.GetInternalValue());
    }
};

//----------------------------------------------------------------------------------------------------------------------

template <>
struct fmt::formatter<Node::Identifier>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        auto const begin = ctx.begin();
        if (begin != ctx.end() && *begin != '}') {  throw format_error("invalid format");}
        return begin;
    }

    template <typename FormatContext>
    auto format(Node::Identifier const& identifier, FormatContext& ctx)
    {
        return format_to(ctx.out(), "{}", identifier.GetNetworkString());
    }
};

//----------------------------------------------------------------------------------------------------------------------

template <>
struct fmt::formatter<Node::SharedIdentifier>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        auto const begin = ctx.begin();
        if (begin != ctx.end() && *begin != '}') {  throw format_error("invalid format");}
        return begin;
    }

    template <typename FormatContext>
    auto format(Node::SharedIdentifier const& spIdentifier, FormatContext& ctx)
    {
        if (!spIdentifier) {
            return format_to(ctx.out(), "[Unknown Identifier]");
        }
        return format_to(ctx.out(), "{}", spIdentifier->GetNetworkString());
    }
};

//----------------------------------------------------------------------------------------------------------------------
