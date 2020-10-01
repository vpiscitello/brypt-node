//------------------------------------------------------------------------------------------------
// File: BryptIdentifier.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier.hpp"
#include "IdentifierDefinitions.hpp"
#include "ReservedIdentifiers.hpp"
#include "../Utilities/Base58.hpp"
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>
//------------------------------------------------------------------------------------------------
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

BryptIdentifier::BufferType GenerateIdentifier(DigestContext const& upDigestContext);

std::string ConvertToNetworkRepresentation(
    DigestContext const& upDigestContext,
    BryptIdentifier::Internal::Type const& identifier);

std::string ConvertToNetworkRepresentation(
    DigestContext const& upDigestContext,
    BryptIdentifier::BufferType const& identifier);

bool Shake128(
    EVP_MD_CTX* pDigestContext,
    BryptIdentifier::BufferType const& source,
    BryptIdentifier::BufferType& destination,
    std::uint32_t length);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

std::string BryptIdentifier::Generate()
{
    //Setup the hashing context to process the randomly generated bytes
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	if (ERR_get_error() != 0 || upDigestContext == nullptr) {
		return {};
	}

    BufferType const identifier = local::GenerateIdentifier(upDigestContext);
    return local::ConvertToNetworkRepresentation(upDigestContext, identifier);
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::Internal::Type> BryptIdentifier::ConvertToInternalRepresentation(
    BryptIdentifier::BufferType const& buffer)
{
    if (buffer.size() != Internal::PayloadSize) {
        return {};
    }

    BryptIdentifier::Internal::Type identifier = {};
    boost::multiprecision::import_bits(identifier, buffer.begin(), buffer.end());
    return identifier;
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::Internal::Type> BryptIdentifier::ConvertToInternalRepresentation(
    std::string_view identifier)
{
    // Verify the identifer's metadata works with this version of the Brypt node
    if (auto const result = identifier.compare(
        0, Network::Metadata.size(), Network::Metadata); result != 0) {
        return {};
    }

    auto const seperatorPosition = identifier.find_first_of(Network::MetadataSeperator);
    assert(seperatorPosition != std::string::npos); // If the metadata was not found, how did we get here?

    std::string_view const payload = identifier.substr(
        seperatorPosition + 1,
        identifier.size() - seperatorPosition);

    BufferType buffer = Base58::Decode(payload);
    BufferType identifierBytes(buffer.begin(), buffer.end() - Internal::ChecksumSize);
    BufferType receivedChecksumBytes(buffer.end() - Internal::ChecksumSize, buffer.end());

    //Setup the hashing context to process the randomly generated bytes
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	if (ERR_get_error() != 0 || upDigestContext == nullptr) {
        return {};
	}

    BufferType generatedChecksumBytes(Internal::ChecksumSize, 0x00);
    local::Shake128(upDigestContext.get(), identifierBytes, generatedChecksumBytes, Internal::ChecksumSize);
    if(generatedChecksumBytes.empty()) {
        return {};
    }

    auto const result = CRYPTO_memcmp(
        generatedChecksumBytes.data(),
        receivedChecksumBytes.data(),
        generatedChecksumBytes.size());

	if (result != 0) {
        return {};
	}
    
    return ConvertToInternalRepresentation(identifierBytes);
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::Network::Type> BryptIdentifier::ConvertToNetworkRepresentation(
    Internal::Type const& identifier)
{
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (ERR_get_error() != 0 || upDigestContext == nullptr) {
        return {};
    }

    return local::ConvertToNetworkRepresentation(upDigestContext, identifier);
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::Network::Type> BryptIdentifier::ConvertToNetworkRepresentation(
    BufferType const& identifier)
{
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
    if (ERR_get_error() != 0 || upDigestContext == nullptr) {
        return {};
    }

    return local::ConvertToNetworkRepresentation(upDigestContext, identifier);
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer::CContainer()
    : m_internalRepresentation(ReservedIdentifiers::Internal::Invalid)
    , m_networkRepresentation(ReservedIdentifiers::Network::Invalid)
    , m_valid(false)
{
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer::CContainer(Internal::Type const& identifier)
    : m_internalRepresentation(ReservedIdentifiers::Internal::Invalid)
    , m_networkRepresentation(ReservedIdentifiers::Network::Invalid)
    , m_valid(false)
{
    SetupFromInternalRepresentation(identifier);
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer::CContainer(std::string_view identifier)
    : m_internalRepresentation(ReservedIdentifiers::Internal::Invalid)
    , m_networkRepresentation(ReservedIdentifiers::Network::Invalid)
    , m_valid(false)
{
    SetupFromNetworkRepresentation(identifier);
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer::CContainer(BufferType const& buffer, BufferContentType type)
    : m_internalRepresentation(ReservedIdentifiers::Internal::Invalid)
    , m_networkRepresentation(ReservedIdentifiers::Network::Invalid)
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
            SetupFromNetworkRepresentation({pData, buffer.size()});
        } break;
        default: assert(false); break;
    }
}

//------------------------------------------------------------------------------------------------

bool BryptIdentifier::CContainer::operator<(CContainer const& other) const
{
    return (m_internalRepresentation < other.m_internalRepresentation);
}

//------------------------------------------------------------------------------------------------
 
bool BryptIdentifier::CContainer::operator==(CContainer const& other) const
{
    if (m_internalRepresentation != other.m_internalRepresentation) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------

bool BryptIdentifier::CContainer::operator!=(CContainer const& other) const
{
    return !operator==(other);
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::Internal::Type BryptIdentifier::CContainer::GetInternalRepresentation() const
{
    return m_internalRepresentation;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::Network::Type BryptIdentifier::CContainer::GetNetworkRepresentation() const
{
    return m_networkRepresentation;
}

//------------------------------------------------------------------------------------------------

std::uint32_t BryptIdentifier::CContainer::NetworkRepresentationSize() const
{
    return m_networkRepresentation.size();
}

//------------------------------------------------------------------------------------------------

bool BryptIdentifier::CContainer::IsValid() const
{
    return m_valid;
}

//------------------------------------------------------------------------------------------------

void BryptIdentifier::CContainer::SetupFromInternalRepresentation(Internal::Type const& identifier)
{
    if (!ReservedIdentifiers::IsIdentifierAllowed(identifier)) {
        return;
    }

    if (auto const optNetworkRepresentation = ConvertToNetworkRepresentation(identifier); optNetworkRepresentation) {
        m_internalRepresentation = identifier;
        m_networkRepresentation = *optNetworkRepresentation;
        m_valid = true;
        assert(m_internalRepresentation == ConvertToInternalRepresentation(m_networkRepresentation));
    }
}

//------------------------------------------------------------------------------------------------

void BryptIdentifier::CContainer::SetupFromNetworkRepresentation(std::string_view identifier)
{
    if (!ReservedIdentifiers::IsIdentifierAllowed(identifier)) {
        return;
    }

    if (auto const optInternalRepresentation = ConvertToInternalRepresentation(identifier); optInternalRepresentation) {
        m_internalRepresentation = *optInternalRepresentation;
        m_networkRepresentation = identifier;
        m_valid = true;
        assert(m_internalRepresentation == ConvertToInternalRepresentation(m_networkRepresentation));
    }
}

//------------------------------------------------------------------------------------------------

std::ostream& BryptIdentifier::operator<<(
    std::ostream& stream,
    BryptIdentifier::CContainer const& identifier)
{
    stream << identifier.m_networkRepresentation;
    return stream;
}

//------------------------------------------------------------------------------------------------

std::stringstream& BryptIdentifier::operator<<(
    std::stringstream& stream,
    BryptIdentifier::CContainer const& identifier)
{
    stream << identifier.m_networkRepresentation;
    return stream;
}

//------------------------------------------------------------------------------------------------

std::stringstream& BryptIdentifier::operator<<(
    std::stringstream& stream,
    BryptIdentifier::SharedContainer const& spIdentifier)
{
    if (!spIdentifier) {
        return stream;
    }
    stream << spIdentifier->m_networkRepresentation;
    return stream;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::BufferType local::GenerateIdentifier(DigestContext const& upDigestContext)
{
    BryptIdentifier::BufferType identifier(BryptIdentifier::Internal::PayloadSize, 0x00);
    do {
        // Get 128 random bits to generate the brypt identifier
        BryptIdentifier::BufferType source(BryptIdentifier::Internal::PayloadSize, 0x00);
        if (!RAND_bytes(source.data(), BryptIdentifier::Internal::PayloadSize) || ERR_get_error() != 0) {
            return {};
        }

        // Hash the random bytes using Shake128 into a buffer. The goal here is to have 
        // a sufficiently random identifer with low likely-hood of collision on the
        // Brypt network. 
        local::Shake128(upDigestContext.get(), source, identifier, BryptIdentifier::Internal::PayloadSize);
        if(identifier.empty()) {
            return {};
        }
    } while(ReservedIdentifiers::IsIdentifierReserved(identifier));

    return identifier;
}

//------------------------------------------------------------------------------------------------

std::string local::ConvertToNetworkRepresentation(
    DigestContext const& upDigestContext,
    BryptIdentifier::Internal::Type const& identifier)
{
    BryptIdentifier::BufferType bytes;
    boost::multiprecision::export_bits(identifier, std::back_inserter(bytes), 8);

    BryptIdentifier::BufferType buffer(BryptIdentifier::Internal::PayloadSize - bytes.size(), 0x00);
    std::copy(bytes.begin(), bytes.end(), std::back_inserter(buffer));
    
    return local::ConvertToNetworkRepresentation(upDigestContext, buffer);
}

//------------------------------------------------------------------------------------------------

std::string local::ConvertToNetworkRepresentation(
    DigestContext const& upDigestContext,
    BryptIdentifier::BufferType const& identifier)
{
    BryptIdentifier::BufferType buffer(identifier);

    // Generate a simple checksum of the identifier. This is not for cryptographic security and 
    // used to verify the correct identifier has been received. 
    BryptIdentifier::BufferType checksum(BryptIdentifier::Internal::ChecksumSize, 0x00);
    local::Shake128(upDigestContext.get(), buffer, checksum, BryptIdentifier::Internal::ChecksumSize);
    if(checksum.empty()) {
        return {};
    }
    
    buffer.insert(buffer.end(), checksum.begin(), checksum.end());

    std::string paylod = {};
    // Prefix the identifier with brypt information
    paylod.append(BryptIdentifier::Network::Metadata);
    // Use Base58 encoding for safe network transfer and a human readible identifier. 
    Base58::Encode(buffer, paylod);

    return paylod;
}

//------------------------------------------------------------------------------------------------

bool local::Shake128(
    EVP_MD_CTX* pDigestContext,
    BryptIdentifier::BufferType const& source,
    BryptIdentifier::BufferType& destination,
    std::uint32_t length)
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

//------------------------------------------------------------------------------------------------