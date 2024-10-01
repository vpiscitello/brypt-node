//----------------------------------------------------------------------------------------------------------------------
// File: CipherPackage.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "CipherPackage.hpp"
#include "Components/Message/PackUtils.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/conf.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;
using MessageAuthenticationCodeContext = std::unique_ptr<EVP_MAC_CTX, decltype(&EVP_MAC_CTX_free)>;
using DigestContext = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Security::CipherSuite::CipherSuite(
    ConfidentialityLevel level, std::string_view agreement, std::string_view cipher, std::string_view hash)
    : m_level(level)
    , m_agreement(agreement)
    , m_cipher(cipher)
    , m_hash(hash)
    , m_encryptionKeySize(0)
    , m_initializationVectorSize(0)
    , m_blockSize(0)
    , m_doesCipherPadInput(false)
    , m_isAuthenticatedCipher(false)
    , m_needsGeneratedInitializationVector(true)
    , m_tagSize(0)
    , m_signatureSize(0)
    , m_upCipherGenerator(EVP_CIPHER_fetch(nullptr, cipher.data(), ""))
    , m_upDigestGenerator(EVP_MD_fetch(nullptr, hash.data(), ""))
{
    if (m_agreement.empty() || m_cipher.empty() || m_hash.empty()) {
        throw std::runtime_error("Provided invalid parameters when initializing the cipher suite!");
    }

    if (!m_upCipherGenerator) {
        throw std::runtime_error("Failed to initialize a valid cipher envelope using the provided cipher name!");
    }

    if (!m_upDigestGenerator) {
        throw std::runtime_error("Failed to initialize a valid digest envelope using the provided hash function name!");
    }

    m_encryptionKeySize = static_cast<std::size_t>(EVP_CIPHER_key_length(m_upCipherGenerator.get()));
    m_initializationVectorSize = static_cast<std::size_t>(EVP_CIPHER_iv_length(m_upCipherGenerator.get()));
    m_blockSize =  static_cast<std::size_t>(EVP_CIPHER_block_size(m_upCipherGenerator.get()));
    m_signatureSize = static_cast<std::size_t>(EVP_MD_size(m_upDigestGenerator.get()));
    
    m_doesCipherPadInput = [&] () {
        switch (EVP_CIPHER_mode(m_upCipherGenerator.get())) {
            case EVP_CIPH_CBC_MODE: [[fallthrough]];
            case EVP_CIPH_ECB_MODE: return true;
            default: return false;
        }
    }();
    
    if (m_isAuthenticatedCipher = EVP_CIPHER_flags(m_upCipherGenerator.get()) & EVP_CIPH_FLAG_AEAD_CIPHER; m_isAuthenticatedCipher) {
        m_needsGeneratedInitializationVector = false;

        // Create an OpenSSL encryption context. 
        local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
        if (!upCipherContext) {
            throw std::runtime_error("Failed to create a cipher context needed to fetch the default tag size!");
        }

        if (EVP_EncryptInit_ex(upCipherContext.get(), m_upCipherGenerator.get(), nullptr, nullptr, nullptr) <= 0) {
            throw std::runtime_error("Failed to initialize a cipher context needed to fetch the default tag size!");
        }

        m_tagSize = EVP_CIPHER_CTX_tag_length(upCipherContext.get());
        if (m_tagSize == 0) {
            m_tagSize = 16; // Default to 256 bits. 
        }

        // List of ciphers that require manual IV or nonce setting
        static std::array<std::string, 4> const ManualIVNeededCiphers = { "ccm", "ocb", "chacha" };
        for (auto const& name : ManualIVNeededCiphers) {
            if (m_cipher.find(name) != std::string::npos) {
                m_needsGeneratedInitializationVector = true;
                break;
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

Security::CipherSuite::CipherSuite(CipherSuite const& other)
    : m_level(other.m_level)
    , m_agreement(other.m_agreement)
    , m_cipher(other.m_cipher)
    , m_hash(other.m_hash)
    , m_encryptionKeySize(other.m_encryptionKeySize)
    , m_initializationVectorSize(other.m_initializationVectorSize)
    , m_blockSize(other.m_blockSize)
    , m_doesCipherPadInput(other.m_doesCipherPadInput)
    , m_isAuthenticatedCipher(other.m_isAuthenticatedCipher)
    , m_needsGeneratedInitializationVector(other.m_needsGeneratedInitializationVector)
    , m_tagSize(other.m_tagSize)
    , m_upCipherGenerator(EVP_CIPHER_fetch(nullptr, m_cipher.data(), ""))
    , m_upDigestGenerator(EVP_MD_fetch(nullptr, m_hash.data(), ""))
    , m_signatureSize(other.m_signatureSize)
{
    if (!m_upCipherGenerator) {
        throw std::runtime_error("Failed to initialize a valid cipher envelope using the provided cipher name!");
    }

    if (!m_upDigestGenerator) {
        throw std::runtime_error("Failed to initialize a valid digest envelope using the provided hash function name!");
    }
}

//----------------------------------------------------------------------------------------------------------------------

Security::CipherSuite::CipherSuite(CipherSuite&& other) noexcept
    : m_level(std::move(other.m_level))
    , m_agreement(std::move(other.m_agreement))
    , m_cipher(std::move(other.m_cipher))
    , m_hash(std::move(other.m_hash))
    , m_encryptionKeySize(std::move(other.m_encryptionKeySize))
    , m_initializationVectorSize(std::move(other.m_initializationVectorSize))
    , m_blockSize(std::move(other.m_blockSize))
    , m_doesCipherPadInput(std::move(other.m_doesCipherPadInput))
    , m_isAuthenticatedCipher(std::move(other.m_isAuthenticatedCipher))
    , m_needsGeneratedInitializationVector(std::move(other.m_needsGeneratedInitializationVector))
    , m_tagSize(std::move(other.m_tagSize))
    , m_signatureSize(std::move(other.m_signatureSize))
    , m_upCipherGenerator(std::move(other.m_upCipherGenerator))
    , m_upDigestGenerator(std::move(other.m_upDigestGenerator))
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::CipherSuite& Security::CipherSuite::operator=(CipherSuite const& other)
{
    m_level = other.m_level;
    m_agreement = other.m_agreement;
    m_cipher = other.m_cipher;
    m_hash = other.m_hash;
    m_encryptionKeySize = other.m_encryptionKeySize;
    m_initializationVectorSize = other.m_initializationVectorSize;
    m_blockSize = other.m_blockSize;
    m_doesCipherPadInput = other.m_doesCipherPadInput;
    m_isAuthenticatedCipher = other.m_isAuthenticatedCipher;
    m_needsGeneratedInitializationVector = other.m_needsGeneratedInitializationVector;
    m_tagSize = other.m_tagSize;
    m_signatureSize = other.m_signatureSize;

    m_upCipherGenerator = CipherAlgorithm{ EVP_CIPHER_fetch(nullptr, m_cipher.data(), "") };
    if (!m_upCipherGenerator) {
        throw std::runtime_error("Failed to initialize a valid cipher envelope using the provided cipher name!");
    }

    m_upDigestGenerator = DigestAlgorithm{ EVP_MD_fetch(nullptr, m_hash.data(), "") };
    if (!m_upDigestGenerator) {
        throw std::runtime_error("Failed to initialize a valid digest envelope using the provided hash function name!");
    }

    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Security::CipherSuite& Security::CipherSuite::operator=(CipherSuite&& other) noexcept
{
    m_level = std::move(other.m_level);
    m_agreement = std::move(other.m_agreement);
    m_cipher = std::move(other.m_cipher);
    m_hash = std::move(other.m_hash);
    m_encryptionKeySize = std::move(other.m_encryptionKeySize);
    m_initializationVectorSize = std::move(other.m_initializationVectorSize);
    m_blockSize = std::move(other.m_blockSize);
    m_doesCipherPadInput = std::move(other.m_doesCipherPadInput);
    m_isAuthenticatedCipher = std::move(other.m_isAuthenticatedCipher);
    m_needsGeneratedInitializationVector = std::move(other.m_needsGeneratedInitializationVector);
    m_tagSize = std::move(other.m_tagSize);
    m_signatureSize = std::move(other.m_signatureSize);
    m_upCipherGenerator = std::move(other.m_upCipherGenerator);
    m_upDigestGenerator = std::move(other.m_upDigestGenerator);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::CipherSuite::operator==(CipherSuite const& other) const noexcept
{
    return m_level == other.m_level && m_agreement == other.m_agreement && m_cipher == other.m_cipher && m_hash == other.m_hash;
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Security::CipherSuite::operator<=>(CipherSuite const& other) const noexcept
{
    if (auto const result = m_level <=> other.m_level; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_agreement <=> other.m_agreement; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_cipher <=> other.m_cipher; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_hash <=> other.m_hash; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::CipherSuite::GetEncryptedSize(std::size_t size) const
{
    assert(m_upCipherGenerator);
    if (!m_upCipherGenerator || size == 0) { return std::size_t{ 0 }; }

    std::size_t encryptedSize = size + GetInitializationVectorSize();
    
    // If the cipher mode pads the input, add enough for the final block
    if (m_doesCipherPadInput) {
        encryptedSize += m_blockSize - (size % m_blockSize);
    }

    // If the cipher is authenticated, add enough for the tag. 
    if (m_isAuthenticatedCipher) {
        encryptedSize += m_tagSize; 
    }

    return encryptedSize;
}

//----------------------------------------------------------------------------------------------------------------------

Security::CipherPackage::CipherPackage(CipherSuite suite, KeyStore&& store)
    : m_suite(std::move(suite))
    , m_store(std::move(store))
    , m_upMacGenerator(EVP_MAC_fetch(nullptr, "hmac", nullptr))
{
    if (!m_upMacGenerator) {
        throw std::runtime_error("Failed to obtain the message authentication code algorithm!"); 
    }
}

//----------------------------------------------------------------------------------------------------------------------

Security::CipherPackage::CipherPackage(CipherPackage&& other) noexcept
    : m_suite(std::move(other.m_suite))
    , m_store(std::move(other.m_store))
    , m_upMacGenerator(std::move(other.m_upMacGenerator))
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::CipherPackage& Security::CipherPackage::operator=(CipherPackage&& other) noexcept
{
    m_suite = std::move(other.m_suite);
    m_store = std::move(other.m_store);
    m_upMacGenerator = std::move(other.m_upMacGenerator);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Security::CipherSuite const& Security::CipherPackage::GetSuite() const { return m_suite; }

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::CipherPackage::Encrypt(ReadableView plaintext) const
{
    // Ensure the caller is able to encrypt the plaintext with generated session keys. 
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Unable to encrypt without generated session keys!"); 
    }

    Security::Buffer ciphertext;
    if (Encrypt(plaintext, ciphertext)) {
        return ciphertext;
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::CipherPackage::Encrypt(ReadableView plaintext, Buffer& destination) const
{
    // Ensure the caller is able to encrypt the buffer with generated session keys. 
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Unable to encrypt without generated session keys!"); 
    }

    // If the buffer contains no data or is less than the specified data to encrypt there is nothing
    // that can be done. 
    if (plaintext.empty()) {
        return false;
    }

    // Create an OpenSSL encryption context. 
    local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!upCipherContext) {
        return false;
    }

    // Get our content encryption key to be used in the cipher. 
    auto const& optKey = m_store.GetContentKey();
    if (!optKey) {
        return false;
    }
    assert(optKey->GetSize() == m_suite.GetEncryptionKeySize());

    auto pContext = upCipherContext.get();
    auto pCipher = m_suite.GetCipher().get();
    std::size_t const keySize = optKey->GetSize();
    std::size_t const ivSize = m_suite.GetInitializationVectorSize();

    {
        std::array<OSSL_PARAM, 3> params = { 
            OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN, const_cast<size_t*>(&keySize)),
            OSSL_PARAM_construct_end(), // Placeholder for iv size 
            OSSL_PARAM_construct_end()
        };

        if (m_suite.IsAuthenticatedCipher()) {
            // Authenticated encryption ciphers need to have initialize just the context and set the iv length first. 
            params[1] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_IVLEN, const_cast<size_t*>(&ivSize));
        } else {
            params[1] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, const_cast<size_t*>(&ivSize));
        }

        if (EVP_EncryptInit_ex2(pContext, pCipher, nullptr, nullptr, params.data()) <= 0) {
            return false;
        }
    }

    std::size_t const encryptedSize = m_suite.GetEncryptedSize(plaintext.size());
    std::size_t const initialBufferSize = destination.size();
    destination.resize(initialBufferSize + encryptedSize); // Extend the destination buffer to contain our ciphertext, it is prefixed with the initialization vector;
    auto pCiphertext = destination.data() + initialBufferSize; // Grab a pointer for the space starting after the initialization vector. 
    auto const pPlaintext = plaintext.data();
    
    std::uint8_t const* pInitializationVector = nullptr;
    if (m_suite.NeedsGeneratedInitializationVector()) {
        // Generate a random initialization vector and write it into the ciphertext buffer to be returned to the caller. 
        std::size_t const ivStart = encryptedSize - ivSize - m_suite.GetTagSize();
        if (!GenerateRandomData(std::span{ destination.begin() + initialBufferSize + ivStart, ivSize })) {
            return false; // If we have failed to generate a random initialization vector, an error has occurred. 
        }
        pInitializationVector = destination.data() + initialBufferSize + ivStart;
    }

    // Initialize the OpenSSL cipher using with the encryption key and IV. 
    if (EVP_EncryptInit_ex(pContext, nullptr, nullptr, optKey->GetData(), pInitializationVector) <= 0) {
        return false;
    }

    // Encrypt the plaintext into the ciphertext buffer.
    constexpr std::size_t MaximumBlockSize = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    std::size_t encrypted = 0;
    std::int32_t block = static_cast<std::int32_t>(std::min(plaintext.size(), MaximumBlockSize));
    for (std::size_t encrypting = plaintext.size(); encrypting > 0; encrypting -= block) {
        std::int32_t processed = 0;
        if (EVP_EncryptUpdate(pContext, pCiphertext, &processed, pPlaintext, block) <= 0) {
            return false;
        }
        encrypted += processed;
        block = static_cast<std::int32_t>(std::min(encrypting, MaximumBlockSize));
    }

    // Cleanup the OpenSSL encryption cipher. 
    {
        std::int32_t processed = 0;
        if (EVP_EncryptFinal_ex(pContext, pCiphertext + encrypted, &processed) <= 0) {
            return false;
        }
        encrypted += processed;
    }

    // If we have not manually generated an initialization vector, then we need to fetch it from the OpenSSL 
    // provider and write it into the ciphertext buffer.
    if (!m_suite.NeedsGeneratedInitializationVector()) { 
        if (EVP_CIPHER_CTX_get_original_iv(pContext, destination.data() + initialBufferSize + encrypted, ivSize) <= 0) {
            return false;
        }
    }

    // If this is an authenticated cipher, append the tag to the ciphertext buffer. 
    if (m_suite.IsAuthenticatedCipher()) {
        std::array<OSSL_PARAM, 2> params = { 
            OSSL_PARAM_construct_octet_string(
                OSSL_CIPHER_PARAM_AEAD_TAG, destination.data() + initialBufferSize + encrypted + ivSize, m_suite.GetTagSize()),
            OSSL_PARAM_construct_end()
        };

        if (EVP_CIPHER_CTX_get_params(pContext, params.data()) <= 0) {
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::CipherPackage::Decrypt(ReadableView buffer) const
{
    // Ensure the caller is able to encrypt the buffer with generated session keys. 
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Unable to decrypt without generated session keys!"); 
    }

    // If the buffer contains no data or is less than the specified data to decrypt there is nothing that can be done.
    if (buffer.empty()) {
        return {};
    }

    // Create an OpenSSL decryption context.
    local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
    if (!upCipherContext) { return {}; }

    // Get the peer's content decryption key to be used in the cipher.
    auto const& optKey = m_store.GetPeerContentKey();
    if (!optKey) { return {}; }
    assert(optKey->GetSize() == m_suite.GetEncryptionKeySize());

    auto pContext = upCipherContext.get();
    auto pCipher = m_suite.GetCipher().get();
    std::size_t const keySize = optKey->GetSize();
    std::size_t const ivSize = m_suite.GetInitializationVectorSize();

    {
        std::array<OSSL_PARAM, 3> params = { 
            OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_KEYLEN, const_cast<size_t*>(&keySize)),
            OSSL_PARAM_construct_end(), // Placeholder for iv size 
            OSSL_PARAM_construct_end()
        };

        if (m_suite.IsAuthenticatedCipher()) {
            // Authenticated encryption ciphers need to have initialize just the context and set the iv length first. 
            params[1] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_AEAD_IVLEN, const_cast<size_t*>(&ivSize));
        } else {
            params[1] = OSSL_PARAM_construct_size_t(OSSL_CIPHER_PARAM_IVLEN, const_cast<size_t*>(&ivSize));
        }

        if (EVP_DecryptInit_ex2(pContext, pCipher, nullptr, nullptr, params.data()) <= 0) {
            return {};
        }
    }

    std::size_t const encryptedDataSize = [&] () -> std::size_t {
        std::size_t size = buffer.size();
        size -= ivSize;
        if (m_suite.IsAuthenticatedCipher()) {
            size -= m_suite.GetTagSize();
        }
        return size;
    }();

    // Setup the initialization vector from the given nonce. 
    auto begin = buffer.begin() + encryptedDataSize;
    auto const end = buffer.end();
    
    Buffer iv;
    if (!PackUtils::UnpackChunk(begin, end, iv, ivSize)) { return {}; }

    if (m_suite.IsAuthenticatedCipher()) {
        Buffer tag;
        if (!PackUtils::UnpackChunk(begin, end, tag, m_suite.GetTagSize())) { return {}; }

        std::array<OSSL_PARAM, 2> params = { 
            OSSL_PARAM_construct_octet_string(OSSL_CIPHER_PARAM_AEAD_TAG, tag.data(), m_suite.GetTagSize()),
            OSSL_PARAM_construct_end()
        };

        if (EVP_CIPHER_CTX_set_params(pContext, params.data()) <= 0) {
            return {};
        }
    }

    // Initialize the OpenSSL cipher using AES-256-CTR with the decryption key and IV.
    if (EVP_DecryptInit_ex(pContext, nullptr, nullptr, optKey->GetData(), iv.data()) <= 0) {
        return {};
    }

    // Sanity check that our decryption key and IV are the size expected by OpenSSL.
    assert(std::int32_t(optKey->GetSize()) == EVP_CIPHER_CTX_key_length(pContext));
    assert(std::int32_t(iv.size()) == EVP_CIPHER_CTX_iv_length(pContext));

    Buffer plaintext(encryptedDataSize, 0x00); // Create a buffer to store the decrypted data. 
    auto pPlaintext = plaintext.data();
    auto const pCiphertext = buffer.data();

    // Decrypt the ciphertext into the plaintext buffer.
    constexpr std::size_t MaximumBlockSize = static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
    std::size_t decrypted = 0;
    std::int32_t block = static_cast<std::int32_t>(std::min(encryptedDataSize, MaximumBlockSize));
    for (std::size_t decrypting = encryptedDataSize; decrypting > 0; decrypting -= block) {
        std::int32_t processed = 0;
        if (EVP_DecryptUpdate(pContext, pPlaintext, &processed, pCiphertext, block) <= 0) {
            return {};
        }
        decrypted += processed;
        block = static_cast<std::int32_t>(std::min(decrypting, MaximumBlockSize));
    }

    // Cleanup the OpenSSL encryption cipher. 
    {
        std::int32_t processed = 0;
        if (EVP_DecryptFinal_ex(pContext, pPlaintext + decrypted, &processed) <= 0) {
            return {};
        }
        if (processed > 0) {
            decrypted += m_suite.GetBlockSize();
        }
    }


    return plaintext;
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::CipherPackage::Sign(Buffer& buffer) const
{
    return Sign(buffer, buffer); // Generate and add the signature to the provided buffer. 
}

//----------------------------------------------------------------------------------------------------------------------

Security::VerificationStatus Security::CipherPackage::Verify(ReadableView buffer) const
{
    // Ensure the caller is able to encrypt the buffer with generated session keys. 
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Unable to verify without generated session keys!"); 
    }

    // Determine the amount of non-signature data is packed into the buffer. 
    std::int64_t const contentSize = buffer.size() - m_suite.GetSignatureSize();
    if (buffer.empty() || contentSize <= 0) { 
        return VerificationStatus::Failed;
    }

    // Get the peer's signature key to be used to generate the expected signature.
    auto const& optKey = m_store.GetPeerSignatureKey();
    if (!optKey) {
        return VerificationStatus::Failed;
    }
    assert(optKey->GetSize() == m_suite.GetSignatureSize());

    // Create the signature that peer should have provided. 
    ReadableView payload(buffer.begin(), contentSize);
    auto const optGeneratedBuffer = GenerateSignature(*optKey, payload);
    if (!optGeneratedBuffer) {
        return VerificationStatus::Failed;
    }

    // Compare the generated signature with the signature attached to the buffer. 
    auto const result = CRYPTO_memcmp(
        optGeneratedBuffer->data(), buffer.data() + contentSize, optGeneratedBuffer->size());

    // If the signatures are not equal than the peer did not sign the buffer or the buffer was altered in transmission. 
    return (result == 0) ? VerificationStatus::Success : VerificationStatus::Failed;
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::CipherPackage::Sign(ReadableView source, Buffer& destination) const
{
    // Ensure the caller is able to encrypt the buffer with generated session keys. 
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Unable to sign without generated session keys!"); 
    }

    // Get our signature key to be used when generating the content signature.
    auto const& optKey = m_store.GetSignatureKey();
    if (!optKey) {
        return false; // If there is no signature key, an error has occurred. 
    }

    assert(optKey->GetSize() == m_suite.GetSignatureSize());

    auto const optSignature = GenerateSignature(*optKey, source);
    if (!optSignature) {
        return false; // If we failed to generate a signature, an error has occurred. 
    }

    // Insert the signature to create a verifiable buffer. 
    destination.insert(destination.end(), optSignature->begin(), optSignature->end());

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::CipherPackage::GenerateSignature(SignatureKey const& key, ReadableView source) const
{
    // If there is no data to be signed, there is nothing to do. 
    if (key.IsEmpty() || source.empty()) {
        return {};
    }

    // Setup the context required for the OpenSSL KDF using the fetched function name.
    local::MessageAuthenticationCodeContext upContext(EVP_MAC_CTX_new(m_upMacGenerator.get()), &EVP_MAC_CTX_free);
    if (!upContext) {
        return {};
    }

    // Setup the parameters for the key derivation. Note: The OpenSSL interface only supports taking non-const values, however, they 
    // are only read as the parameters are constructed. 
    std::array<OSSL_PARAM, 3> params = {
        OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, const_cast<char*>(EVP_MD_name(m_suite.GetDigest().get())), 0),
        OSSL_PARAM_construct_octet_string(OSSL_MAC_PARAM_KEY, const_cast<std::uint8_t*>(key.GetData()), key.GetSize()),
        OSSL_PARAM_construct_end()
    };

    // Set the parameters on the context such that they will be used when the keys are derived.
    if (EVP_MAC_init(upContext.get(), nullptr, 0, params.data()) <= 0) {
        return {};
    }

    if (EVP_MAC_update(upContext.get(), source.data(), source.size()) <= 0) {
        return {};
    }

    Buffer signature(m_suite.GetSignatureSize(), 0x00);
    std::size_t hashed = 0;
    if (EVP_MAC_final(upContext.get(), signature.data(), &hashed, m_suite.GetSignatureSize()) <= 0) {
        return {};
    }

    return signature;
}

//----------------------------------------------------------------------------------------------------------------------
