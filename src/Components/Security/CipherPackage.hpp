//----------------------------------------------------------------------------------------------------------------------
// File: CipherPackage.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "KeyStore.hpp"
#include "SecurityTypes.hpp"
#include "SecurityDefinitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string_view>
#include <string>
//----------------------------------------------------------------------------------------------------------------------

typedef struct evp_cipher_st EVP_CIPHER;
typedef struct evp_mac_st EVP_MAC;
typedef struct evp_md_st EVP_MD;

extern "C" void EVP_CIPHER_free(EVP_CIPHER*);
extern "C" void EVP_MAC_free(EVP_MAC*);
extern "C" void EVP_MD_free(EVP_MD*);

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

class CipherSuite;
class CipherPackage;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::CipherSuite
{
public:
    struct CipherAlgorithmDeleter
    {
        void operator()(EVP_CIPHER* pCipher) const
        {
            EVP_CIPHER_free(pCipher);
        }
    };

    using CipherAlgorithm = std::unique_ptr<EVP_CIPHER, CipherAlgorithmDeleter>;

    struct DigestAlgorithmDeleter
    {
        void operator()(EVP_MD* pDigest) const
        {
            EVP_MD_free(pDigest);
        }
    };

    using DigestAlgorithm = std::unique_ptr<EVP_MD, DigestAlgorithmDeleter>;

    CipherSuite(ConfidentialityLevel level, std::string_view agreement, std::string_view cipher, std::string_view hash);

    CipherSuite(CipherSuite const& other);
    CipherSuite(CipherSuite&& other) noexcept;
    CipherSuite& operator=(CipherSuite const& other);
    CipherSuite& operator=(CipherSuite&& other) noexcept;

    [[nodiscard]] bool operator==(CipherSuite const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(CipherSuite const& other) const noexcept;

    [[nodiscard]] ConfidentialityLevel GetConfidentialityLevel() const { return m_level; }
    [[nodiscard]] std::string const& GetKeyAgreementName() const { return m_agreement; }
    [[nodiscard]] std::string const& GetCipherName() const { return m_cipher; }
    
    [[nodiscard]] std::size_t GetEncryptionKeySize() const { return m_encryptionKeySize; }
    [[nodiscard]] std::size_t GetInitializationVectorSize() const { return m_initializationVectorSize; }
    [[nodiscard]] bool DoesCipherPadInput() const { return m_doesCipherPadInput; }
    [[nodiscard]] bool IsAuthenticatedCipher() const { return m_isAuthenticatedCipher; }
    [[nodiscard]] bool NeedsGeneratedInitializationVector() const { return m_needsGeneratedInitializationVector; }
    [[nodiscard]] std::size_t GetBlockSize() const { return m_blockSize; }
    [[nodiscard]] std::size_t GetTagSize() const { return m_tagSize; }
    [[nodiscard]] std::size_t GetEncryptedSize(std::size_t size) const;

    [[nodiscard]] std::string const& GetHashFunctionName() const { return m_hash; }
    [[nodiscard]] std::size_t GetSignatureKeySize() const { return m_signatureSize; }
    [[nodiscard]] std::size_t GetSignatureSize() const { return m_signatureSize; }

    [[nodiscard]] CipherAlgorithm const& GetCipher() const { return m_upCipherGenerator; }
    [[nodiscard]] DigestAlgorithm const& GetDigest() const { return m_upDigestGenerator; }

private:
    ConfidentialityLevel m_level;
    std::string m_agreement;
    std::string m_cipher;
    std::string m_hash;
    std::size_t m_encryptionKeySize;
    std::size_t m_initializationVectorSize;
    std::size_t m_blockSize;
    bool m_doesCipherPadInput;
    bool m_isAuthenticatedCipher;
    bool m_needsGeneratedInitializationVector;
    std::size_t m_tagSize;
    std::size_t m_signatureSize;

    CipherAlgorithm m_upCipherGenerator;
    DigestAlgorithm m_upDigestGenerator;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::CipherPackage
{
public:
    CipherPackage(CipherSuite suite, KeyStore&& store);

    CipherPackage(CipherPackage const& other) = delete;
    CipherPackage& operator=(CipherPackage const& other) = delete;

    CipherPackage(CipherPackage&& other) noexcept;
    CipherPackage& operator=(CipherPackage&& other) noexcept;

    [[nodiscard]] CipherSuite const& GetSuite() const;

    [[nodiscard]] OptionalBuffer Encrypt(ReadableView plaintext) const;
    [[nodiscard]] bool Encrypt(ReadableView plaintext, Buffer& destination) const;
    [[nodiscard]] OptionalBuffer Decrypt(ReadableView ciphertext) const;
    [[nodiscard]] bool Sign(Buffer& destination) const;
    [[nodiscard]] bool Sign(ReadableView source, Buffer& destination) const;
    [[nodiscard]] VerificationStatus Verify(ReadableView buffer) const;

private:
    struct MessageAuthenticationCodeDeleter
    {
        void operator()(EVP_MAC* pMac) const
        {
            EVP_MAC_free(pMac);
        }
    };

    using MessageAuthenticator = std::unique_ptr<EVP_MAC, MessageAuthenticationCodeDeleter>;

    [[nodiscard]] OptionalBuffer GenerateSignature(SignatureKey const& key, ReadableView source) const;

    CipherSuite m_suite;
    KeyStore m_store;
    MessageAuthenticator m_upMacGenerator;
};

//----------------------------------------------------------------------------------------------------------------------
