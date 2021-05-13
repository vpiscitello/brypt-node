//----------------------------------------------------------------------------------------------------------------------
// File: BryptIdentifier.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier.hpp"
#include "IdentifierDefinitions.hpp"
#include "ReservedIdentifiers.hpp"
#include "Utilities/Base58.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

using Buffer = std::vector<std::uint8_t>;
using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

Buffer GenerateIdentifier(DigestContext const& upDigestContext);

std::string ConvertToNetworkRepresentation(
    DigestContext const& upDigestContext, Node::Internal::Identifier::Type const& identifier);
std::string ConvertToNetworkRepresentation(DigestContext const& upDigestContext, Buffer const& identifier);

bool Shake128(EVP_MD_CTX* pDigestContext, Buffer const& source, Buffer& destination, std::uint32_t length);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

std::string Node::GenerateIdentifier()
{
    //Setup the hashing context to process the randomly generated bytes
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	if (ERR_get_error() != 0 || upDigestContext == nullptr) { return {}; }

    local::Buffer const identifier = local::GenerateIdentifier(upDigestContext);
    return local::ConvertToNetworkRepresentation(upDigestContext, identifier);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Internal::Identifier::Type> Node::ConvertToInternalRepresentation(
    local::Buffer const& buffer)
{
    if (buffer.size() != Internal::Identifier::PayloadSize) { return {}; }

    Node::Internal::Identifier::Type identifier = {};
    boost::multiprecision::import_bits(identifier, buffer.begin(), buffer.end());
    return identifier;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Internal::Identifier::Type> Node::ConvertToInternalRepresentation(
    std::string_view identifier)
{
    // Verify the identifer's metadata works with this version of the Brypt node
    if (auto const result = identifier.compare(
        0, Network::Identifier::Metadata.size(), Network::Identifier::Metadata); result != 0) { return {}; }

    auto const seperatorPosition = identifier.find_first_of(Network::Identifier::MetadataSeperator);
    assert(seperatorPosition != std::string::npos); // If the metadata was not found, how did we get here?

    std::string_view const payload = identifier.substr(seperatorPosition + 1, identifier.size() - seperatorPosition);

    local::Buffer buffer = Base58::Decode(payload);
    local::Buffer identifierBytes(buffer.begin(), buffer.end() - Internal::Identifier::ChecksumSize);
    local::Buffer receivedChecksumBytes(buffer.end() - Internal::Identifier::ChecksumSize, buffer.end());

    //Setup the hashing context to process the randomly generated bytes
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	if (ERR_get_error() != 0 || upDigestContext == nullptr) { return {}; }

    local::Buffer generatedChecksumBytes(Internal::Identifier::ChecksumSize, 0x00);
    local::Shake128(upDigestContext.get(), identifierBytes, generatedChecksumBytes, Internal::Identifier::ChecksumSize);
    if(generatedChecksumBytes.empty()) { return {}; }

    auto const result = CRYPTO_memcmp(
        generatedChecksumBytes.data(), receivedChecksumBytes.data(), generatedChecksumBytes.size());
	if (result != 0) { return {}; }
    
    return ConvertToInternalRepresentation(identifierBytes);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Network::Identifier::Type> Node::ConvertToNetworkRepresentation(
    Internal::Identifier::Type const& identifier)
{
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (ERR_get_error() != 0 || upDigestContext == nullptr) { return {}; }

    return local::ConvertToNetworkRepresentation(upDigestContext, identifier);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Network::Identifier::Type> Node::ConvertToNetworkRepresentation(
    local::Buffer const& identifier)
{
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (ERR_get_error() != 0 || upDigestContext == nullptr) { return {}; }

    return local::ConvertToNetworkRepresentation(upDigestContext, identifier);
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier::Identifier()
    : m_internal(Node::Internal::Identifier::Invalid)
    , m_network(Node::Network::Identifier::Invalid)
    , m_valid(false)
{
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier::Identifier(Internal::Identifier::Type const& identifier)
    : m_internal(Node::Internal::Identifier::Invalid)
    , m_network(Node::Network::Identifier::Invalid)
    , m_valid(false)
{
    SetupFromInternalRepresentation(identifier);
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier::Identifier(std::string_view identifier)
    : m_internal(Node::Internal::Identifier::Invalid)
    , m_network(Node::Network::Identifier::Invalid)
    , m_valid(false)
{
    SetupFromNetworkRepresentation(identifier);
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier::Identifier(local::Buffer const& buffer, BufferContentType type)
    : m_internal(Node::Internal::Identifier::Invalid)
    , m_network(Node::Network::Identifier::Invalid)
    , m_valid(false)
{
    switch (type) {
        case BufferContentType::Internal: {
            if (auto const optIdentifier = ConvertToInternalRepresentation(buffer); optIdentifier) {
                SetupFromInternalRepresentation(*optIdentifier);
            }
        } break;
        case BufferContentType::Network: {
            auto const pData = reinterpret_cast<char const*>(buffer.data());
            SetupFromNetworkRepresentation({ pData, buffer.size() });
        } break;
        default: assert(false); break;
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::Identifier::operator<(Identifier const& other) const
{
    if (m_valid != other.m_valid) { return m_valid < other.m_valid; }
    return m_internal < other.m_internal;
}

//----------------------------------------------------------------------------------------------------------------------
 
bool Node::Identifier::operator==(Identifier const& other) const
{
    if (!m_valid && !other.m_valid) { return false; } // Invalid identifiers don't compare equal
    return m_internal == other.m_internal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::Identifier::operator!=(Identifier const& other) const
{
    return !operator==(other);
}

//----------------------------------------------------------------------------------------------------------------------

Node::Internal::Identifier::Type Node::Identifier::GetInternalValue() const
{
    return m_internal;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Network::Identifier::Type const& Node::Identifier::GetNetworkString() const
{
    return m_network;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Node::Identifier::NetworkStringSize() const
{
    return m_network.size();
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::Identifier::IsValid() const
{
    return m_valid;
}

//----------------------------------------------------------------------------------------------------------------------

void Node::Identifier::SetupFromInternalRepresentation(Internal::Identifier::Type const& identifier)
{
    if (!Node::IsIdentifierAllowed(identifier)) { return; }

    if (auto const optNetworkRepresentation = ConvertToNetworkRepresentation(identifier); optNetworkRepresentation) {
        m_internal = identifier;
        m_network = *optNetworkRepresentation;
        m_valid = true;
        assert(m_internal == ConvertToInternalRepresentation(m_network));
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Node::Identifier::SetupFromNetworkRepresentation(std::string_view identifier)
{
    if (!Node::IsIdentifierAllowed(identifier)) { return; }

    if (auto const optInternalRepresentation = ConvertToInternalRepresentation(identifier); optInternalRepresentation) {
        m_internal = *optInternalRepresentation;
        m_network = identifier;
        m_valid = true;
        assert(m_internal == ConvertToInternalRepresentation(m_network));
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::ostream& Node::operator<<(
    std::ostream& stream, Node::Identifier const& identifier)
{
    stream << identifier.m_network;
    return stream;
}

//----------------------------------------------------------------------------------------------------------------------

std::stringstream& Node::operator<<(
    std::stringstream& stream, Node::Identifier const& identifier)
{
    stream << identifier.m_network;
    return stream;
}

//----------------------------------------------------------------------------------------------------------------------

std::stringstream& Node::operator<<(
    std::stringstream& stream, Node::SharedIdentifier const& spIdentifier)
{
    if (!spIdentifier) {
        stream << "[Unknown Identifier]";
    } else {
        stream << spIdentifier->m_network;
    }
    return stream;
}

//----------------------------------------------------------------------------------------------------------------------

local::Buffer local::GenerateIdentifier(DigestContext const& upDigestContext)
{
    local::Buffer identifier(Node::Internal::Identifier::PayloadSize, 0x00);

    do {
        // Get 128 random bits to generate the brypt identifier
        local::Buffer source(Node::Internal::Identifier::PayloadSize, 0x00);
        if (!RAND_bytes(source.data(), Node::Internal::Identifier::PayloadSize) || ERR_get_error() != 0) { return {}; }

        // Hash the random bytes using Shake128 into a buffer. The goal here is to have  a sufficiently random identifer
        // with low likely-hood of collision on the Brypt network. 
        local::Shake128(upDigestContext.get(), source, identifier, Node::Internal::Identifier::PayloadSize);
        if(identifier.empty()) { return {}; }
    } while(Node::IsIdentifierReserved(identifier));

    return identifier;
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::ConvertToNetworkRepresentation(
    DigestContext const& upDigestContext, Node::Internal::Identifier::Type const& identifier)
{
    local::Buffer bytes;
    boost::multiprecision::export_bits(identifier, std::back_inserter(bytes), 8);

    local::Buffer buffer(Node::Internal::Identifier::PayloadSize - bytes.size(), 0x00);
    std::copy(bytes.begin(), bytes.end(), std::back_inserter(buffer));
    
    return local::ConvertToNetworkRepresentation(upDigestContext, buffer);
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::ConvertToNetworkRepresentation(
    DigestContext const& upDigestContext, local::Buffer const& identifier)
{
    local::Buffer buffer(identifier);

    // Generate a simple checksum of the identifier. This is not for cryptographic security and used to verify the 
    // correct identifier has been received. 
    local::Buffer checksum(Node::Internal::Identifier::ChecksumSize, 0x00);
    local::Shake128(upDigestContext.get(), buffer, checksum, Node::Internal::Identifier::ChecksumSize);
    if(checksum.empty()) { return {}; }
    
    buffer.insert(buffer.end(), checksum.begin(), checksum.end());

    std::string paylod = {};
    // Prefix the identifier with brypt information
    paylod.append(Node::Network::Identifier::Metadata);
    // Use Base58 encoding for safe network transfer and a human readible identifier. 
    Base58::Encode(buffer, paylod);

    return paylod;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::Shake128(
    EVP_MD_CTX* pDigestContext, local::Buffer const& source, local::Buffer& destination, std::uint32_t length)
{
    if (EVP_DigestInit_ex(pDigestContext, EVP_shake128(), nullptr) != 1 || ERR_get_error() != 0) {
        destination.clear();
        return false;
    }

    if (EVP_DigestUpdate(pDigestContext, source.data(), source.size()) != 1 || ERR_get_error() != 0) {
        destination.clear();
        return false;
    }

    if (EVP_DigestFinalXOF(pDigestContext, destination.data(), length) != 1 || ERR_get_error() != 0) {
        destination.clear();
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
