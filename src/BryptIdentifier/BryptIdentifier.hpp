//------------------------------------------------------------------------------------------------
// File: BryptIdentifier.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "IdentifierTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace BryptIdentifier {
//------------------------------------------------------------------------------------------------

enum class BufferContentType : std::uint8_t { Internal, Network };

struct Hasher;

std::string Generate();

std::optional<Internal::Type> ConvertToInternalRepresentation(
    std::vector<std::uint8_t> const& buffer);
std::optional<Internal::Type> ConvertToInternalRepresentation(std::string_view identifier);

std::optional<Network::Type> ConvertToNetworkRepresentation(Internal::Type const& identifier);
std::optional<Network::Type> ConvertToNetworkRepresentation(
    std::vector<std::uint8_t> const& identifier);

std::ostream& operator<<(
    std::ostream& stream, BryptIdentifier::CContainer const& identifier);
        
std::stringstream& operator<<(
    std::stringstream& stream, BryptIdentifier::CContainer const& identifier);

std::stringstream& operator<<(
    std::stringstream& stream, BryptIdentifier::SharedContainer const& spIdentifier);

//------------------------------------------------------------------------------------------------
} // BryptIdentifier namespace
//------------------------------------------------------------------------------------------------

class BryptIdentifier::CContainer 
{
public:
    CContainer();
    explicit CContainer(Internal::Type const& identifier);
    explicit CContainer(std::string_view identifier);
    explicit CContainer(std::vector<std::uint8_t> const& buffer, BufferContentType type);

    CContainer(CContainer const&) = default;
    CContainer(CContainer&&) = default;
    CContainer& operator=(CContainer const&) = default;
    CContainer& operator=(CContainer&&) = default;

    bool operator<(CContainer const& other) const;
    bool operator==(CContainer const& other) const;
    bool operator!=(CContainer const& other) const;

    friend std::ostream& operator<<(
        std::ostream& stream,
        CContainer const& identifier);
    friend std::stringstream& operator<<(
        std::stringstream& stream,
        CContainer const& identifier);
    friend std::stringstream& operator<<(
        std::stringstream& stream,
        SharedContainer const& spIdentifier);

    Internal::Type GetInternalRepresentation() const;
    Network::Type GetNetworkRepresentation() const;

    std::uint32_t NetworkRepresentationSize() const;
    bool IsValid() const;

private:
    void SetupFromInternalRepresentation(Internal::Type const& identifier);
    void SetupFromNetworkRepresentation(std::string_view identifier);

    BryptIdentifier::Internal::Type m_internalRepresentation;
    BryptIdentifier::Network::Type m_networkRepresentation;
    bool m_valid;

};

//------------------------------------------------------------------------------------------------

struct BryptIdentifier::Hasher  {
    std::size_t operator()(CContainer const& identifier) const
    {
        return std::hash<Internal::Type>()(identifier.GetInternalRepresentation());
    }
};

//------------------------------------------------------------------------------------------------
