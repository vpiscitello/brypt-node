//----------------------------------------------------------------------------------------------------------------------
// File: KeyStore.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "CipherPackage.hpp"
#include "KeyStore.hpp"
#include "SecurityUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/params.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
//----------------------------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
#include <stdexcept>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::size_t MinimumKeySize = 16;

constexpr std::string_view VerificationMessage = "verified";

using KeyDerivationFunctionContext = std::unique_ptr<EVP_KDF_CTX, decltype(&EVP_KDF_CTX_free)>;
using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Security::KeyStore::KeyStore(PublicKey&& publicKey)
    : m_upKeyDerivationFunction(EVP_KDF_fetch(nullptr, "hkdf", nullptr))
    //, m_pVerificationDigest(EVP_shake256())
    , m_pVerificationDigest(EVP_sha3_256())
    , m_verificationDataSize(static_cast<std::size_t>(EVP_MD_size(EVP_sha3_256())))
    , m_publicKey(std::move(publicKey))
    , m_optPeerPublicKey()
    , m_salt()
    , m_optPrincipalKey()
    , m_optContentKey()
    , m_optPeerContentKey()
    , m_optSignatureKey()
    , m_optPeerSignatureKey()
    , m_bHasGeneratedKeys(false)
{
    if (!m_upKeyDerivationFunction) {
        throw std::runtime_error("Failed to obtain the key derivation function!"); 
    }

    if (m_publicKey.IsEmpty()) {
        throw std::runtime_error("Invalid public key was provided when creating the key store!"); 
    }

    // Generate our principal salt for the session. 
    auto const optPrincipalSalt = GenerateRandomData(PrincipalRandomSize);
    if (!optPrincipalSalt) {
        throw std::runtime_error("Failed to generate initial salt when creating the key store!"); 
    }

    m_salt.Append(*optPrincipalSalt);
}

//----------------------------------------------------------------------------------------------------------------------

Security::KeyStore::KeyStore(KeyStore&& other) noexcept
    : m_upKeyDerivationFunction(std::move(other.m_upKeyDerivationFunction))
    , m_pVerificationDigest(std::move(other.m_pVerificationDigest))
    , m_verificationDataSize(std::move(other.m_verificationDataSize))
    , m_publicKey(std::exchange(other.m_publicKey, {}))
    , m_optPeerPublicKey(std::exchange(other.m_optPeerPublicKey, std::nullopt))
    , m_salt(std::exchange(other.m_salt, {}))
    , m_optPrincipalKey(std::exchange(other.m_optPrincipalKey, std::nullopt))
    , m_optContentKey(std::exchange(other.m_optContentKey, std::nullopt))
    , m_optPeerContentKey(std::exchange(other.m_optPeerContentKey, std::nullopt))
    , m_optSignatureKey(std::exchange(other.m_optSignatureKey, std::nullopt))
    , m_optPeerSignatureKey(std::exchange(other.m_optPeerSignatureKey, std::nullopt))
    , m_bHasGeneratedKeys(std::exchange(other.m_bHasGeneratedKeys, false))
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::KeyStore& Security::KeyStore::operator=(KeyStore&& other) noexcept
{
    m_upKeyDerivationFunction = std::move(other.m_upKeyDerivationFunction);
    m_pVerificationDigest = std::move(other.m_pVerificationDigest);
    m_verificationDataSize = std::move(other.m_verificationDataSize);
    m_publicKey = std::exchange(other.m_publicKey, {});
    m_optPeerPublicKey = std::exchange(other.m_optPeerPublicKey, std::nullopt);
    m_salt = std::exchange(other.m_salt, {});
    m_optPrincipalKey = std::exchange(other.m_optPrincipalKey, std::nullopt);
    m_optContentKey = std::exchange(other.m_optContentKey, std::nullopt);
    m_optPeerContentKey = std::exchange(other.m_optPeerContentKey, std::nullopt);
    m_optSignatureKey = std::exchange(other.m_optSignatureKey, std::nullopt);
    m_optPeerSignatureKey = std::exchange(other.m_optPeerSignatureKey, std::nullopt);
    m_bHasGeneratedKeys = std::exchange(other.m_bHasGeneratedKeys, false);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Security::KeyStore::~KeyStore()
{
    m_publicKey.Erase();
    m_optPeerPublicKey.reset();
    m_salt.Erase();
    m_optContentKey.reset();
    m_optPeerContentKey.reset();
    m_optSignatureKey.reset();
    m_optPeerSignatureKey.reset();
    m_optPrincipalKey.reset();
}

//----------------------------------------------------------------------------------------------------------------------

Security::PublicKey const& Security::KeyStore::GetPublicKey() const
{
    return m_publicKey;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalPublicKey const& Security::KeyStore::GetPeerPublicKey() const
{
    return m_optPeerPublicKey;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::KeyStore::GetPublicKeySize() const
{
    return m_publicKey.GetSize();
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] Security::Salt const& Security::KeyStore::GetSalt() const
{
    return m_salt;
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::size_t Security::KeyStore::GetSaltSize() const
{
    return m_salt.GetSize();
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalEncryptionKey const& Security::KeyStore::GetContentKey() const
{
    return m_optContentKey;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalEncryptionKey const& Security::KeyStore::GetPeerContentKey() const
{
    return m_optPeerContentKey;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalSignatureKey const& Security::KeyStore::GetSignatureKey() const
{
    return m_optSignatureKey;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalSignatureKey const& Security::KeyStore::GetPeerSignatureKey() const
{
    return m_optPeerSignatureKey;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::KeyStore::GetVerificationDataSize() const
{
    return m_verificationDataSize;
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::KeyStore::HasGeneratedKeys() const
{
    return m_bHasGeneratedKeys;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::KeyStore::SetPeerPublicKey(PublicKey&& publicKey)
{
    m_optPeerPublicKey = std::move(publicKey);
}

//----------------------------------------------------------------------------------------------------------------------

void Security::KeyStore::PrependSessionSalt(Salt salt)
{
    salt.Append(m_salt.GetData());
    m_salt = std::move(salt);
}

//----------------------------------------------------------------------------------------------------------------------

void Security::KeyStore::AppendSessionSalt(Salt const& salt)
{
    m_salt.Append(salt.GetData());
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalSecureBuffer Security::KeyStore::GenerateSessionKeys(
    ExchangeRole role, CipherSuite const& cipherSuite, SharedSecret const& sharedSecret)
{
    if (auto const size = sharedSecret.GetSize(); size < 16 || size > 1024) {
        return {}; // We don't want a shared secret that is too small or might overflow the buffer. 
    }

    auto const& contentKeySize = cipherSuite.GetEncryptionKeySize();
    auto const& signatureKeySize = cipherSuite.GetSignatureKeySize();

    // Calculate the number of bytes needed to generate the keys required for the security strategy.
    std::size_t const principalKeySize = (contentKeySize * 2) + (signatureKeySize * 2);

    // We additional bytes than the caller specifies to generate verification bytes the 
    // strategy can use to verify key sharing was successful. 
    std::size_t const verificationKeySize = static_cast<std::size_t>(EVP_MD_block_size(EVP_sha3_256()));
    std::uint32_t const totalDerivationSize = principalKeySize + verificationKeySize;

    // Setup the context required for the OpenSSL KDF using the fetched function name.
    local::KeyDerivationFunctionContext upContext(EVP_KDF_CTX_new(m_upKeyDerivationFunction.get()), &EVP_KDF_CTX_free);
    if (!upContext) {
        return {};
    }

    // Setup the parameters for the key derivation. Note: The OpenSSL interface only supports taking non-const values, however, they 
    // are only read as the parameters are constructed. 
    std::array<OSSL_PARAM, 5> params = {
        OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, const_cast<char*>("sha3-256"), std::size_t{ 9 }),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, const_cast<std::uint8_t*>(sharedSecret.GetData().data()), sharedSecret.GetSize()),
        OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT, const_cast<std::uint8_t*>(m_salt.GetData().data()), m_salt.GetSize()),
        OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_MAC_SIZE, const_cast<std::uint32_t*>(&totalDerivationSize)),
        OSSL_PARAM_construct_end()
    };

    // Set the parameters on the context such that they will be used when the keys are derived.
    if (EVP_KDF_CTX_set_params(upContext.get(), params.data()) <= 0) {
        return {};
    }

    // Create a secure buffer for the principal key with enough allocated data for the derived key. 
    m_optPrincipalKey.emplace(SecureBuffer{ totalDerivationSize });

    // Use OpenSSL to derive the session key into the writable view of the principal key.
    auto const writable = m_optPrincipalKey->GetData();
    if (EVP_KDF_derive(upContext.get(), writable.data(), writable.size(), nullptr) <= 0) {
        return {};
    }

    // The principal key sectors that correspond to each key remain fixed between roles. 
    // The principal key sectors are: 
    //     1.) Initiator Content Key. 
    //     2.) Acceptor Content Key. 
    //     3.) Initiator Signature Key. 
    //     4.) Acceptor Signature Key. 
    // To keep the key interfaces in sync, the cordons need to be set based on the role of the calling strategy. 
    std::size_t partitioned = 0;
    switch (role) {
        case ExchangeRole::Initiator: {
            partitioned = SetInitiatorKeyCordons(contentKeySize, signatureKeySize);
        } break;
        case ExchangeRole::Acceptor: {
            partitioned = SetAcceptorKeyCordons(contentKeySize, signatureKeySize);
        } break;
        default: assert(false); break; // What is this?
    }

    // Sanity checks to ensure the key partitions are expected. 
    assert(partitioned == principalKeySize);
    assert((m_optPrincipalKey->GetSize() - partitioned) == verificationKeySize);

    // Generate verification data based on the last 32 bytes of the derived principal key. 
    auto optVerificationData = GenerateVerificationData(m_optPrincipalKey->GetCordon(partitioned, verificationKeySize));

    // If for some reason verification data could not be generated the generation of session
    // keys was not successful. 
    if (!optVerificationData) { 
        return {};
    }

    m_bHasGeneratedKeys = true;

    return optVerificationData;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::KeyStore::Erase()
{
    m_publicKey.Erase();
    m_optPeerPublicKey.reset();
    m_salt.Erase();
    m_optContentKey.reset();
    m_optPeerContentKey.reset();
    m_optSignatureKey.reset();
    m_optPeerSignatureKey.reset();
    m_optPrincipalKey.reset();
    m_bHasGeneratedKeys = false;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::KeyStore::SetInitiatorKeyCordons(std::size_t contentKeySize, std::size_t signatureKeySize)
{
    assert(m_optPrincipalKey);
    std::size_t offset = 0;
    
    m_optContentKey = m_optPrincipalKey->GetCordon(offset, contentKeySize);
    offset += contentKeySize;
    
    m_optPeerContentKey = m_optPrincipalKey->GetCordon(offset, contentKeySize);
    offset += contentKeySize;
    
    m_optSignatureKey = m_optPrincipalKey->GetCordon(offset, signatureKeySize);
    offset += signatureKeySize;
    
    m_optPeerSignatureKey = m_optPrincipalKey->GetCordon(offset, signatureKeySize);
    offset += signatureKeySize;
    
    return offset;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::KeyStore::SetAcceptorKeyCordons(std::size_t contentKeySize, std::size_t signatureKeySize)
{
    assert(m_optPrincipalKey);
    std::size_t offset = 0;
    
    m_optPeerContentKey = m_optPrincipalKey->GetCordon(offset, contentKeySize);
    offset += contentKeySize;
    
    m_optContentKey = m_optPrincipalKey->GetCordon(offset, contentKeySize);
    offset += contentKeySize;
    
    m_optPeerSignatureKey = m_optPrincipalKey->GetCordon(offset, signatureKeySize);
    offset += signatureKeySize;
    
    m_optSignatureKey = m_optPrincipalKey->GetCordon(offset, signatureKeySize);
    offset += signatureKeySize;
    
    return offset;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalSecureBuffer Security::KeyStore::GenerateVerificationData(ReadableView key)
{
    local::DigestContext upDigestContext(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	if (!upDigestContext) {
        return {};
    }

    if (EVP_DigestInit_ex(upDigestContext.get(), m_pVerificationDigest, nullptr) <= 0) {
        return {};
    }

    // Make a verification data buffer to hash the verification key and a verification seed. 
    SecureBuffer data{ key, local::VerificationMessage };
    auto const writeable = data.GetData();

    // If for some reason the buffer could not be loaded into OpenSSL, the buffer data should be wiped. 
    if (EVP_DigestUpdate(upDigestContext.get(), writeable.data(), writeable.size()) <= 0) {
        return {};
    }

    data.Resize(m_verificationDataSize);  // Ensure the key buffer is the correct size and can be written to.  

    // If for some reason the verification data could not be written, the buffer data should be wiped. 
    std::uint32_t processed = 0;
    if (EVP_DigestFinal_ex(upDigestContext.get(), writeable.data(), &processed) <= 0) {
        return {};
    }

    if (processed != m_verificationDataSize) {
        return {}; // If for some reason we did not process the expected amount of data, an error has occurred. 
    }

    return data;
}

//----------------------------------------------------------------------------------------------------------------------
