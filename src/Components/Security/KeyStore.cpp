//------------------------------------------------------------------------------------------------
// File: KeyStore.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "KeyStore.hpp"
#include "SecurityUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

constexpr std::uint32_t MinimumKeySize = 16;

constexpr std::string_view BaseExpansionSeed = "expansion-seed";

constexpr std::string_view VerificationData = "verify";
constexpr std::uint32_t VerificationKeySize = 32;

using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

bool DeriveSecureKey(Security::Buffer& key, std::uint32_t size);

Security::OptionalBuffer GenerateVerificationData(
    std::uint8_t const* key, std::uint32_t keySize, std::uint32_t dataSize);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

Security::CKeyStore::CKeyStore()
    : m_optPeerPublicKey()
    , m_seed(local::BaseExpansionSeed.begin(), local::BaseExpansionSeed.end())
    , m_optPrinicpalKey()
    , m_optVerificationData()
    , m_optContentKeyCordons()
    , m_optPeerContentKeyCordons()
    , m_optSignatureKeyCordons()
    , m_optPeerSignatureKeyCordons()
    , m_bHasGeneratedKeys(false)
{

}

//------------------------------------------------------------------------------------------------

void Security::CKeyStore::SetPeerPublicKey(Security::Buffer&& buffer)
{
    m_optPeerPublicKey = std::move(buffer);
}

//------------------------------------------------------------------------------------------------

void Security::CKeyStore::ExpandSessionSeed(Security::Buffer const& buffer)
{
    m_seed.insert(m_seed.end(), buffer.begin(), buffer.end());
}

//------------------------------------------------------------------------------------------------

bool Security::CKeyStore::GenerateSessionKeys(
    Security::Role role,
    Security::Buffer&& buffer,
    std::uint32_t contentKeyBytes,
    std::uint32_t signatureKeyBytes)
{
    // Ensure the caller is generating keys that offer at least 128 bits of security. 
    if (contentKeyBytes < local::MinimumKeySize || signatureKeyBytes < local::MinimumKeySize) {
        return false;
    }
    
    // Calculate the number of bytes needed to generate the keys required for the security
    // strategy.
    std::uint32_t const principalKeyBytes = (2 * contentKeyBytes) + (2 * signatureKeyBytes);

    // We additional bytes than the caller specifies to generate verification bytes the 
    // strategy can use to verify key sharing was successful. 
    std::uint32_t const totalDerivationBytes = principalKeyBytes + local::VerificationKeySize;

    // Take ownership of the provided shared secret in order to manager its lifetime. 
    Security::Buffer derivation = std::move(buffer);
    buffer.reserve(totalDerivationBytes);

    // Concatenate the shared secret with the seed. This should ensure the generated keys 
    // are different if the same public/private keys are being used. It is expected that
    // the strategy requires random data as part of key sharing. 
    buffer.insert(buffer.end(), m_seed.begin(), m_seed.end());

    // The shared secret needs to be processed from statistically strong bytes into uniform 
    // cryptographically strong bytes. If for some reason a secure buffer could not be 
    // generated, the generation of session keys was not successful. 
    if (!local::DeriveSecureKey(derivation, totalDerivationBytes)) {
        Security::EraseMemory(derivation.data(), derivation.size());
        return false;
    }

    // Capture the derived principal key buffer into a SecurityBuffer that will ensure the 
    // bytes are wiped at the end of the store's lifetime. 
    m_optPrinicpalKey = Security::CSecureBuffer(std::move(derivation));
    assert(m_optPrinicpalKey->GetSize() == totalDerivationBytes);

    // The princial key sectors that correspond to each key remain fixed between roles. 
    // The principal key sectors are: 
    //     1.) Initiator Content Key. 
    //     2.) Acceptor Content Key. 
    //     3.) Initiator Signature Key. 
    //     4.) Acceptor Signature Key. 
    // To keep the key interfaces in sync, the cordons need to be set based on the role 
    // of the calling strategy. 
    std::uint32_t partitioned = 0;
    switch (role) {
        case Security::Role::Initiator: {
            partitioned = SetInitiatorKeyCordons(contentKeyBytes, signatureKeyBytes);
        } break;
        case Security::Role::Acceptor: {
            partitioned = SetAcceptorKeyCordons(contentKeyBytes, signatureKeyBytes);
        } break;
        default: assert(false); break; // What is this?
    }

    // Sanity checks to ensure the key partitions are expected. 
    assert(partitioned == principalKeyBytes);
    assert((m_optPrinicpalKey->GetSize() - partitioned) == local::VerificationKeySize);

    // Generate verification data based on the last 32 bytes of the derived principal key. 
    m_optVerificationData = local::GenerateVerificationData(
        m_optPrinicpalKey->GetData() + partitioned, 
        local::VerificationKeySize, VerificationSize);

    // If for some reason verification data could not be generated the generation of session
    // keys was not successful. 
    if (!m_optVerificationData) {
        return false;
    }

    m_bHasGeneratedKeys = true;

    return true;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::CKeyStore::GetPeerPublicKey() const
{
    return m_optPeerPublicKey;
}

//------------------------------------------------------------------------------------------------

Security::OptionalKeyResult Security::CKeyStore::GetContentKey() const
{
    if (!m_optPrinicpalKey || !m_optContentKeyCordons) {
        return {};
    }

    auto const& [begin, end] = *m_optContentKeyCordons;
    return Security::KeyResult{ m_optPrinicpalKey->GetData() + begin, std::uint32_t(end - begin) };
}

//------------------------------------------------------------------------------------------------

Security::OptionalKeyResult Security::CKeyStore::GetPeerContentKey() const
{
    if (!m_optPrinicpalKey || !m_optPeerContentKeyCordons) {
        return {};
    }

    auto const& [begin, end] = *m_optPeerContentKeyCordons;
    return Security::KeyResult{ m_optPrinicpalKey->GetData() + begin, std::uint32_t(end - begin) };
}

//------------------------------------------------------------------------------------------------

Security::OptionalKeyResult Security::CKeyStore::GetSignatureKey() const
{
    if (!m_optPrinicpalKey || !m_optSignatureKeyCordons) {
        return {};
    }

    auto const& [begin, end] = *m_optSignatureKeyCordons;
    return Security::KeyResult{ m_optPrinicpalKey->GetData() + begin, std::uint32_t(end - begin) };
}

//------------------------------------------------------------------------------------------------

Security::OptionalKeyResult Security::CKeyStore::GetPeerSignatureKey() const
{
    if (!m_optPrinicpalKey || !m_optPeerSignatureKeyCordons) {
        return {};
    }

    auto const& [begin, end] = *m_optPeerSignatureKeyCordons;
    return Security::KeyResult{ m_optPrinicpalKey->GetData() + begin, std::uint32_t(end - begin) };
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::CKeyStore::GetVerificationData() const
{
    return m_optVerificationData;
}

//------------------------------------------------------------------------------------------------

bool Security::CKeyStore::HasGeneratedKeys() const
{
    return m_bHasGeneratedKeys;
}

//------------------------------------------------------------------------------------------------

void Security::CKeyStore::ResetState()
{
    m_optPeerPublicKey.reset();
    m_seed.clear();
    m_optPrinicpalKey.reset();
    m_optVerificationData.reset();
    m_optContentKeyCordons.reset();
    m_optPeerContentKeyCordons.reset();
    m_optSignatureKeyCordons.reset();
    m_optPeerSignatureKeyCordons.reset();
    m_bHasGeneratedKeys = false;
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::CKeyStore::SetInitiatorKeyCordons(
    std::uint32_t contentKeyBytes, std::uint32_t signatureKeyBytes)
{
    std::uint32_t begin = 0;
    std::uint32_t end = contentKeyBytes;

    m_optContentKeyCordons = { begin, end }; begin = end; end += contentKeyBytes;
    m_optPeerContentKeyCordons = { begin, end }; begin = end; end += signatureKeyBytes;
    m_optSignatureKeyCordons = { begin, end }; begin = end; end += signatureKeyBytes;
    m_optPeerSignatureKeyCordons = { begin, end };

    return end;
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::CKeyStore::SetAcceptorKeyCordons(
    std::uint32_t contentKeyBytes, std::uint32_t signatureKeyBytes)
{
    std::uint32_t begin = 0;
    std::uint32_t end = contentKeyBytes;

    m_optPeerContentKeyCordons = { begin, end }; begin = end; end += contentKeyBytes;
    m_optContentKeyCordons = { begin, end }; begin = end; end += signatureKeyBytes;
    m_optPeerSignatureKeyCordons = { begin, end }; begin = end; end += signatureKeyBytes;
    m_optSignatureKeyCordons = { begin, end };

    return end;
}

//------------------------------------------------------------------------------------------------

bool local::DeriveSecureKey(Security::Buffer& key, std::uint32_t size)
{ 
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	if (ERR_get_error() != 0 || upDigestContext == nullptr) {
        return false;
	}

    if (EVP_DigestInit_ex(upDigestContext.get(), EVP_shake256(), nullptr) != 1 || ERR_get_error() != 0) {
        return false;
    }

    if (EVP_DigestUpdate(upDigestContext.get(), key.data(), key.size()) != 1 || ERR_get_error() != 0) {
        return false;
    }

    key.resize(size); // Expand the key buffer such that data can be written. 

    if (EVP_DigestFinalXOF(upDigestContext.get(), key.data(), size) != 1 || ERR_get_error() != 0) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer local::GenerateVerificationData(
    std::uint8_t const* key, std::uint32_t keySize, std::uint32_t dataSize)
{
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	if (ERR_get_error() != 0 || upDigestContext == nullptr) {
        return {};
	}

    if (EVP_DigestInit_ex(upDigestContext.get(), EVP_shake256(), nullptr) != 1 || ERR_get_error() != 0) {
        return {};
    }

    // Make a verification data buffer to hash the verification key and a verification seed. 
    Security::Buffer data(&key[0], &key[keySize]);
    data.reserve(dataSize);
    data.insert(data.end(), local::VerificationData.begin(), local::VerificationData.end());

    // If for some reason the buffer could not be loaded into OpenSSL, the buffer data should be wiped. 
    if (EVP_DigestUpdate(upDigestContext.get(), data.data(), data.size()) != 1 || ERR_get_error() != 0) {
        Security::EraseMemory(data.data(), data.size());
        return {};
    }

    data.resize(dataSize);  // Expand the key buffer such that data can be written. 

    // If for some reason the verification data could not be written, the buffer data should be wiped. 
    if (EVP_DigestFinalXOF(upDigestContext.get(), data.data(), dataSize) != 1 || ERR_get_error() != 0) {
        Security::EraseMemory(data.data(), data.size());
        return {};
    }

    return data;
}

//------------------------------------------------------------------------------------------------
