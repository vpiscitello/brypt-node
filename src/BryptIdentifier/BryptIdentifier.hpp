//------------------------------------------------------------------------------------------------
// File: BryptIdentifier.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
//------------------------------------------------------------------------------------------------
#include <boost/multiprecision/cpp_int.hpp>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace BryptIdentifier {
//------------------------------------------------------------------------------------------------

using InternalType = boost::multiprecision::uint128_t;
using NetworkType = std::string;

using BufferType = std::vector<std::uint8_t>;
enum class BufferContentType : std::uint8_t { Internal, Network };

constexpr std::string_view const Prefix = "bry";
constexpr std::string_view const Version = "0";
constexpr std::string_view const MetadataSeperator = ":";

constexpr std::string_view const Metadata = "bry0:";
constexpr std::uint8_t const TerminatorByte = 59; // ";"

constexpr std::uint8_t const InternalSize = 16;
constexpr std::uint8_t const ChecksumSize = 4;
constexpr std::uint8_t const MaxNetworkSize = 36;

class CContainer;
using SharedContainer = std::shared_ptr<CContainer const>;
using WeakContainer = std::weak_ptr<CContainer const>;

struct Hasher;

std::string Generate();

std::optional<InternalType> ConvertToInternalRepresentation(BufferType const& buffer);
std::optional<InternalType> ConvertToInternalRepresentation(std::string_view identifier);

std::optional<NetworkType> ConvertToNetworkRepresentation(InternalType const& identifier);
std::optional<NetworkType> ConvertToNetworkRepresentation(BufferType const& identifier);

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
    explicit CContainer(InternalType const& identifier);
    explicit CContainer(std::string_view identifier);
    explicit CContainer(BufferType const& buffer, BufferContentType type);

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

    InternalType GetInternalRepresentation() const;
    NetworkType GetNetworkRepresentation() const;

    std::uint32_t NetworkRepresentationSize() const;
    bool IsValid() const;

private:
    void SetupFromInternalRepresentation(InternalType const& identifier);
    void SetupFromNetworkRepresentation(std::string_view identifier);

    BryptIdentifier::InternalType m_internalRepresentation;
    BryptIdentifier::NetworkType m_networkRepresentation;
    bool m_valid;

};

//------------------------------------------------------------------------------------------------

struct BryptIdentifier::Hasher  {
    std::size_t operator()(CContainer const& identifier) const
    {
        return std::hash<InternalType>()(identifier.GetInternalRepresentation());
    }
};

//------------------------------------------------------------------------------------------------
