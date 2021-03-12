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
namespace BryptIdentifier {
//----------------------------------------------------------------------------------------------------------------------

enum class BufferContentType : std::uint8_t { Internal, Network };

class Container;

struct Hasher;

std::string Generate();

std::optional<Internal::Type> ConvertToInternalRepresentation(
    std::vector<std::uint8_t> const& buffer);
std::optional<Internal::Type> ConvertToInternalRepresentation(std::string_view identifier);

std::optional<Network::Type> ConvertToNetworkRepresentation(Internal::Type const& identifier);
std::optional<Network::Type> ConvertToNetworkRepresentation(
    std::vector<std::uint8_t> const& identifier);

std::ostream& operator<<(
    std::ostream& stream, BryptIdentifier::Container const& identifier);
        
std::stringstream& operator<<(
    std::stringstream& stream, BryptIdentifier::Container const& identifier);

std::stringstream& operator<<(
    std::stringstream& stream, BryptIdentifier::SharedContainer const& spIdentifier);

//----------------------------------------------------------------------------------------------------------------------
} // BryptIdentifier namespace
//----------------------------------------------------------------------------------------------------------------------

class BryptIdentifier::Container 
{
public:
    Container();
    explicit Container(Internal::Type const& identifier);
    explicit Container(std::string_view identifier);
    explicit Container(std::vector<std::uint8_t> const& buffer, BufferContentType type);

    Container(Container const&) = default;
    Container(Container&&) = default;
    Container& operator=(Container const&) = default;
    Container& operator=(Container&&) = default;

    bool operator<(Container const& other) const;
    bool operator==(Container const& other) const;
    bool operator!=(Container const& other) const;

    friend std::ostream& operator<<(
        std::ostream& stream,
        Container const& identifier);
    friend std::stringstream& operator<<(
        std::stringstream& stream,
        Container const& identifier);
    friend std::stringstream& operator<<(
        std::stringstream& stream,
        SharedContainer const& spIdentifier);

    Internal::Type GetInternalRepresentation() const;
    Network::Type GetNetworkRepresentation() const;

    std::size_t NetworkRepresentationSize() const;
    bool IsValid() const;

private:
    void SetupFromInternalRepresentation(Internal::Type const& identifier);
    void SetupFromNetworkRepresentation(std::string_view identifier);

    BryptIdentifier::Internal::Type m_internalRepresentation;
    BryptIdentifier::Network::Type m_networkRepresentation;
    bool m_valid;

};

//----------------------------------------------------------------------------------------------------------------------

struct BryptIdentifier::Hasher  {
    std::size_t operator() (Container const& identifier) const
    {
        return std::hash<Internal::Type>()(identifier.GetInternalRepresentation());
    }
};

//----------------------------------------------------------------------------------------------------------------------

template <>
struct fmt::formatter<BryptIdentifier::Container>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        auto const begin = ctx.begin();
        if (begin != ctx.end() && *begin != '}') {  throw format_error("invalid format");}
        return begin;
    }

    template <typename FormatContext>
    auto format(BryptIdentifier::Container const& identifier, FormatContext& ctx)
    {
        return format_to(ctx.out(), "{}", identifier.GetNetworkRepresentation());
    }
};

//----------------------------------------------------------------------------------------------------------------------

template <>
struct fmt::formatter<BryptIdentifier::SharedContainer>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        auto const begin = ctx.begin();
        if (begin != ctx.end() && *begin != '}') {  throw format_error("invalid format");}
        return begin;
    }

    template <typename FormatContext>
    auto format(BryptIdentifier::SharedContainer const& spIdentifier, FormatContext& ctx)
    {
        if (!spIdentifier) {
            return format_to(ctx.out(), "[Unknown Identifier]");
        }
        return format_to(ctx.out(), "{}", spIdentifier->GetNetworkRepresentation());
    }
};

//----------------------------------------------------------------------------------------------------------------------
