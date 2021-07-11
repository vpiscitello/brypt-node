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

std::optional<Internal::Identifier> ToInternalIdentifier(std::vector<std::uint8_t> const& buffer);
std::optional<Internal::Identifier> ToInternalIdentifier(std::string_view identifier);

std::optional<External::Identifier> ToExternalIdentifier(Internal::Identifier const& identifier);
std::optional<External::Identifier> ToExternalIdentifier(std::vector<std::uint8_t> const& identifier);

std::ostream& operator<<(std::ostream& stream, Node::Identifier const& identifier);
std::stringstream& operator<<(std::stringstream& stream, Node::Identifier const& identifier);
std::stringstream& operator<<(std::stringstream& stream, Node::SharedIdentifier const& spIdentifier);

//----------------------------------------------------------------------------------------------------------------------
} // Node namespace
//----------------------------------------------------------------------------------------------------------------------

class Node::Identifier 
{
public:
    static constexpr std::uint8_t const PayloadBytes = 16;
    static constexpr std::uint8_t const ChecksumBytes = 4;

    static constexpr std::string_view Prefix = "bry";
    static constexpr std::string_view Version = "0";
    static constexpr std::string_view MetadataSeperator = ":";
    static constexpr std::string_view Metadata = "bry0:";
    static constexpr std::uint32_t MinimumSize = 31;
    static constexpr std::uint32_t MaximumSize = 33;

    Identifier();
    explicit Identifier(Internal::Identifier const& identifier);
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

    operator Internal::Identifier const&() const;
    operator External::Identifier const&() const;
    std::size_t Size() const;
    bool IsValid() const;

private:
    void FromInternalIdentifier(Internal::Identifier const& identifier);
    void FromExternalIdentifier(std::string_view identifier);

    Internal::Identifier m_internal;
    External::Identifier m_network;
    bool m_valid;
};

//----------------------------------------------------------------------------------------------------------------------

struct Node::Hasher  {
    std::size_t operator() (Identifier const& identifier) const
    {
        return std::hash<Internal::Identifier>()(identifier);
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
        return format_to(ctx.out(), "{}", static_cast<Node::External::Identifier const&>(identifier));
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
        return format_to(ctx.out(), "{}", static_cast<Node::External::Identifier const&>(*spIdentifier));
    }
};

//----------------------------------------------------------------------------------------------------------------------
