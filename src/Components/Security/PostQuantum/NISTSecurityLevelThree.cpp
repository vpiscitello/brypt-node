//------------------------------------------------------------------------------------------------
// File: NISTSecurityLevelThree.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "NISTSecurityLevelThree.hpp"
#include "../ConnectDefinitions.hpp"
#include "../../BryptMessage/PackUtils.hpp"
#include "../../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <optional>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

// Various size constants required for AES-256-CTR 
constexpr std::uint32_t EncryptionKeySize = 32; // In bytes, 256 bits. 
constexpr std::uint32_t EncryptionIVSize = 16; // In bytes, 128 bits. 
constexpr std::uint32_t EncryptionBlockSize = 16; // In bytes, 128 bits. 

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

Security::OptionalBuffer GenerateRandomData(std::uint32_t size);

Security::Buffer GenerateEncapsulationMessage(
    Security::Buffer const& encapsulation, Security::Buffer const& data);
bool ParseEncapsulationMessage(
    Security::Buffer const& buffer, Security::Buffer& encapsulation, Security::Buffer& data);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

CPQNISTL3Strategy::CPQNISTL3Strategy()
    : m_role(Security::Role::Unknown)
    , m_synchronization(
        { 1, Security::SynchronizationStatus::Processing })
    , m_kem(KeyEncapsulationSchme.data())
    , m_store()
{
    m_store.SetPublicKey(m_kem.generate_keypair());
}

//------------------------------------------------------------------------------------------------

Security::Strategy CPQNISTL3Strategy::GetStrategyType() const
{
    return Type;
}

//------------------------------------------------------------------------------------------------

void CPQNISTL3Strategy::SetRole(Security::Role role)
{
    m_role = role;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CPQNISTL3Strategy::SynchronizationStages() const
{
    switch (m_role) {
        case Security::Role::Initiator: return InitiatorSynchronizationStages;
        case Security::Role::Acceptor: return AcceptorSynchronizationStages;
        default: assert(false); return 0;
    }
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationStatus CPQNISTL3Strategy::SynchronizationStatus() const
{
    return m_synchronization.status;
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult CPQNISTL3Strategy::Synchronize(Security::Buffer const& buffer)
{
    if (m_role == Security::Role::Unknown) {
        throw std::runtime_error("The Security Strategy cannot be used without first setting the role!");
    }

    switch (m_role) {
        case Security::Role::Initiator: return HandleInitiatorSynchronization(buffer);
        case Security::Role::Acceptor: return HandleAcceptorSynchronization(buffer);
        default: {
            m_synchronization.stage = 0;
            m_synchronization.status = Security::SynchronizationStatus::Error;
            return { m_synchronization.status, {} };
        }
    }
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer CPQNISTL3Strategy::Encrypt(
    Security::Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const
{
    // Ensure the caller is able to encrypt the buffer with generated session keys. 
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot encrypt before synchronization is complete!");
    }

    // If the buffer contains no data or is less than the specified data to encrypt there is nothing
    // that can be done. 
    if (buffer.size() == 0 || buffer.size() < size) {
		return {};
	}

    // Create an OpenSSL encryption context. 
	local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
	if (ERR_get_error() != 0 || upCipherContext == nullptr) {
		return {};
	}

    // Get our content encryption key to be used in the cipher. 
    auto const optEncryptionKey = m_store.GetContentKey();
    if (!optEncryptionKey) {
		return {};
    }

    // Destructure the KeyStore result into meaniful names. 
    auto const& [pKey, keySize] = *optEncryptionKey;
    assert(keySize == local::EncryptionKeySize);

    // Setup the AES-256-CTR initalization vector from the given nonce. 
	std::array<std::uint8_t, local::EncryptionIVSize> iv = {};
	std::memcpy(iv.data(), &nonce, sizeof(nonce));

    // Initialize the OpenSSL cipher using AES-256-CTR with the encryption key and IV. 
	EVP_EncryptInit_ex(upCipherContext.get(), EVP_aes_256_ctr(), nullptr, pKey, iv.data());
	if (ERR_get_error() != 0) {
		return {};
	}

    // Sanity check that our encryption key and IV are the size expected by OpenSSL. 
    assert(std::int32_t(keySize) == EVP_CIPHER_CTX_key_length(upCipherContext.get()));
    assert(std::int32_t(iv.size()) == EVP_CIPHER_CTX_iv_length(upCipherContext.get()));

	Message::Buffer ciphertext(size, 0x00); // Create a buffer to store the encrypted data. 
	auto pCiphertext = reinterpret_cast<std::uint8_t*>(ciphertext.data());
	auto const pPlaintext = reinterpret_cast<std::uint8_t const*>(buffer.data());

    // Encrypt the plaintext into the ciphertext buffer. 
	std::int32_t encrypted = 0;
	EVP_EncryptUpdate(upCipherContext.get(), pCiphertext, &encrypted, pPlaintext, size);
	if (ERR_get_error() != 0 || encrypted == 0) {
		return {};
	}

    // Cleanup the OpenSSL encryption cipher. 
	EVP_EncryptFinal_ex(upCipherContext.get(), pCiphertext + encrypted, &encrypted);
	if (ERR_get_error() != 0) {
		return {};
	}

	return ciphertext;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer CPQNISTL3Strategy::Decrypt(
    Security::Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const
{
    // Ensure the caller is able to decrypt the buffer with generated session keys.
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot decrypt before synchronization is complete!");
    }

    // If the buffer contains no data or is less than the specified data to decrypt there is nothing
    // that can be done.
	if (size == 0) {
		return {};
	}

    // Create an OpenSSL decryption context.
	local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
	if (ERR_get_error() != 0 || upCipherContext == nullptr) {
		return {};
	}

    // Get the peer's content decryption key to be used in the cipher.
    auto const optDecryptionKey = m_store.GetPeerContentKey();
    if (!optDecryptionKey) {
		return {};
    }

    // Destructure the KeyStore result into meaniful names.
    auto const& [pKey, keySize] = *optDecryptionKey;
    assert(keySize == local::EncryptionKeySize);

    // Setup the AES-256-CTR initalization vector from the given nonce.
	std::array<std::uint8_t, local::EncryptionIVSize> iv = {};
	std::memcpy(iv.data(), &nonce, sizeof(nonce));

    // Initialize the OpenSSL cipher using AES-256-CTR with the decryption key and IV.
	EVP_DecryptInit_ex(upCipherContext.get(), EVP_aes_256_ctr(), nullptr, pKey, iv.data());
	if (ERR_get_error() != 0) {
		return {};
	}

    // Sanity check that our decryption key and IV are the size expected by OpenSSL.
    assert(std::int32_t(keySize) == EVP_CIPHER_CTX_key_length(upCipherContext.get()));
    assert(std::int32_t(iv.size()) == EVP_CIPHER_CTX_iv_length(upCipherContext.get()));

	Message::Buffer plaintext(size, 0x00); // Create a buffer to store the decrypted data. 
	auto pPlaintext = reinterpret_cast<std::uint8_t*>(plaintext.data());
	auto const pCiphertext = reinterpret_cast<std::uint8_t const*>(buffer.data());

    // Decrypt the ciphertext into the plaintext buffer.
	std::int32_t decrypted = 0;
	EVP_DecryptUpdate(upCipherContext.get(), pPlaintext, &decrypted, pCiphertext, size);
	if (ERR_get_error() != 0 || decrypted == 0) {
		return {};
	}

    // Cleanup the OpenSSL decryption cipher.
	EVP_DecryptFinal_ex(upCipherContext.get(), pPlaintext + decrypted ,&decrypted);
	if (ERR_get_error() != 0) {
		return {};
	}

	return plaintext;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CPQNISTL3Strategy::Sign(Security::Buffer& buffer) const
{
    // Ensure the caller is able to sign the buffer with generated session keys.
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot decrypt before synchronization is complete!");
    }
    
    // Get our signature key to be used when generating the content siganture. .
    auto const optSignatureKey = m_store.GetSignatureKey();
    if (!optSignatureKey) {
		return 0;
    }

    // Destructure the KeyStore result into meaniful names. 
    auto const [pKey, keySize] = *optSignatureKey;
    assert(keySize == SignatureLength);

    // Sign the provided buffer with our signature key .
    Security::OptionalBuffer optSignature = GenerateSignature(pKey, keySize, buffer.data(), buffer.size());
    if (!optSignature) {
        return 0;
    }

    // Insert the signature to create a verifiable buffer. 
    buffer.insert(buffer.end(), optSignature->begin(), optSignature->end());
    return optSignature->size(); // Tell the caller how many bytes have been inserted. 
}

//------------------------------------------------------------------------------------------------

Security::VerificationStatus CPQNISTL3Strategy::Verify(Security::Buffer const& buffer) const
{
    // Ensure the caller is able to verify the buffer with generated session keys.
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot decrypt before synchronization is complete!");
    }

    // Determine the amount of non-signature data is packed into the buffer. 
    std::int64_t packContentSize = buffer.size() - SignatureLength;
	if (buffer.empty() || packContentSize <= 0) {
		return Security::VerificationStatus::Failed;
	}

    // Get the peer's signature key to be used to generate the expected siganture..
    auto const optPeerSignatureKey = m_store.GetPeerSignatureKey();
    if (!optPeerSignatureKey) {
		return Security::VerificationStatus::Failed;
    }

    // Destructure the KeyStore result into meaniful names. 
    auto const [pKey, keySize] = *optPeerSignatureKey;
    assert(keySize == SignatureLength);

    // Create the signature that peer should have provided. 
	auto const optGeneratedBuffer = GenerateSignature(pKey, keySize, buffer.data(), packContentSize);
	if (!optGeneratedBuffer) {
		return Security::VerificationStatus::Failed;
	}

    // Compare the generated signature with the signature attached to the buffer. 
	auto const result = CRYPTO_memcmp(
		optGeneratedBuffer->data(), buffer.data() + packContentSize, optGeneratedBuffer->size());

    // If the signatures are not equal than the peer did not sign the buffer or the buffer was
    // altered in transmission. 
	if (result != 0) {
		return Security::VerificationStatus::Failed;
	}

	return Security::VerificationStatus::Success;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CPQNISTL3Strategy::GetPublicKeySize() const
{
    auto const& details = m_kem.get_details();
    return details.length_public_key;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer CPQNISTL3Strategy::GenerateSignature(
    std::uint8_t const* pKey,
    std::uint32_t keySize,
    std::uint8_t const* pData,
    std::uint32_t dataSize) const
{
    // If there is no data to be signed, there is nothing to do. 
	if (dataSize == 0) {
		return {};
	}

    // Hash the provided buffer with the provided key to generate the signature. 
	std::uint32_t hashed = 0;
	auto const signature = HMAC(EVP_sha384(), pKey, keySize, pData, dataSize, nullptr, &hashed);
	if (ERR_get_error() != 0 || hashed == 0) {
		return {};
	}

    // Provide the caller a buffer created from the c-style signature. 
    return Security::Buffer(&signature[0], &signature[hashed]);
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult CPQNISTL3Strategy::HandleInitiatorSynchronization(
    Security::Buffer const& buffer)
{
    //         Stage       |            Input            |           Output            |
    //     Initialization  | PublicKey + PrincipalRandom | PublicKey + PrincipalRandom |
    //     Decapsulation   | Encapsulation + SyncMessage |         SyncMessage         |
    switch (static_cast<InitiatorSynchronizationStage>(m_synchronization.stage))
    {
        case InitiatorSynchronizationStage::Initialization: {
            return HandleInitiatorInitialization(buffer);
        }
        case InitiatorSynchronizationStage::Decapsulation: {
            return HandleInitiatorDecapsulation(buffer);
        }
        // It is an error to be called in all other synchrinization stages. 
        case InitiatorSynchronizationStage::Complete:
        case InitiatorSynchronizationStage::Invalid: {
            m_synchronization.status = Security::SynchronizationStatus::Error;
            return { m_synchronization.status, {} };
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult CPQNISTL3Strategy::HandleInitiatorInitialization(
    Security::Buffer const& buffer)
{
    // Handle processing the synchronization buffer.
    {
        auto const& details = m_kem.get_details();
        
        // If the provided buffer does not contain the 
        if (std::uint32_t const expected = (details.length_public_key + PrincipalRandomLength);
            buffer.size() != expected) {
            m_synchronization.status = Security::SynchronizationStatus::Error;
            return { m_synchronization.status, {} };
        }

        m_store.SetPeerPublicKey({ buffer.begin(), buffer.begin() + details.length_public_key });
        m_store.ExpandSessionSeed({ buffer.end() - PrincipalRandomLength, buffer.end() });
    }

    auto const optPrincipalSeed = local::GenerateRandomData(PrincipalRandomLength);
    if (!optPrincipalSeed) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    m_store.ExpandSessionSeed(*optPrincipalSeed);

    // Get out public key to be provided to the acceptor. If for some reason we do not have a
    // public key this synchronization stage can not be completed. 
    auto const& optPublicKey = m_store.GetPublicKey();
    if (!optPublicKey) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return {m_synchronization.status, {} };
    }

    Security::Buffer response;
    response.reserve(optPublicKey->size() + optPrincipalSeed->size());
    response.insert(response.end(), optPublicKey->begin(), optPublicKey->end());
    response.insert(response.end(), optPrincipalSeed->begin(), optPrincipalSeed->end());

    // We expect to be provided the peer's shared secret encapsulation in the next 
    // synchronization stage. 
    m_synchronization.stage = static_cast<std::uint8_t>(InitiatorSynchronizationStage::Decapsulation);

    return { m_synchronization.status, response };
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult CPQNISTL3Strategy::HandleInitiatorDecapsulation(
    Security::Buffer const& buffer)
{
    // The caller should provide us with a synchronization message generated by an acceptor
    // strategy. The encaped shared secret and verification data needs to be unpacked from 
    // the buffer. If for some reason the buffer cannot be parsed, it is an error. 
    Security::Buffer encapsulation;
    Security::Buffer peerVerificationData;
    if (!local::ParseEncapsulationMessage(buffer, encapsulation, peerVerificationData)) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Attempt to decapsulate the shared secret. If the shared secret could not be decapsulated
    // or the session keys fail to be generated, it is an error. 
    if (!DecapsulateSharedSecret(encapsulation)) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Now that the shared secret has been decapsulated, we can verify that the message has been
    // provided by an honest peer. If the provided buffer cannot be verified, it is an error. 
    if (auto const status = Verify(buffer); status != Security::VerificationStatus::Success) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Given the buffer has been verified we can decrypt the provided verification data to 
    // ensure our the session keys align with the peer. 
    auto const optDecryptedData = Decrypt(peerVerificationData, peerVerificationData.size(), 0);
    if (!optDecryptedData) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Get our own verification data to verify the peer's session keys and provide the peer some 
    // data that they can verify that we have generated the proper session keys. 
    auto const& optVerificationData = m_store.GetVerificationData();
    if (!optVerificationData) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Verify the provided verification data matches the verification data we have generated. 
    // If the data does not match, it is an error. 
    bool const bMatchedVerificationData = std::equal(
        optVerificationData->begin(), optVerificationData->end(), optDecryptedData->begin());
    if (!bMatchedVerificationData) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Encrypt our own verification data to challenge the peer's session keys. 
    Security::OptionalBuffer optVerificationMessage = Encrypt(
        *optVerificationData, optVerificationData->size(), 0);

    // If for some reason we cannot sign the verification data it is an error. 
    if (Sign(*optVerificationMessage) == 0) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // The synchronization process is now complete. 
    m_synchronization.stage = static_cast<std::uint8_t>(InitiatorSynchronizationStage::Complete);
    m_synchronization.status = Security::SynchronizationStatus::Ready;

    return {m_synchronization.status, *optVerificationMessage };
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult CPQNISTL3Strategy::HandleAcceptorSynchronization(
    Security::Buffer const& buffer)
{
    //          Stage      |            Input            |           Output            |
    //     Initialization  |            Hello            | PublicKey + PrincipalRandom |
    //     Encapsulation   | PublicKey + PrincipalRandom | Encapsulation + SyncMessage |
    //      Verification   |          SyncMessage        |            Done             |
    switch (static_cast<AcceptorSynchronizationStage>(m_synchronization.stage))
    {
        case AcceptorSynchronizationStage::Initialization: {
            return HandleAcceptorInitialization(buffer);
        }
        case AcceptorSynchronizationStage::Encapsulation: {
            return HandleAcceptorEncapsulation(buffer);
        }
        case AcceptorSynchronizationStage::Verification: {
            return HandleAcceptorVerification(buffer);
        }
        // It is an error to be called in all other synchrinization stages. 
        case AcceptorSynchronizationStage::Complete:
        case AcceptorSynchronizationStage::Invalid: {
            m_synchronization.status = Security::SynchronizationStatus::Error;
            return { m_synchronization.status, {} };
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult CPQNISTL3Strategy::HandleAcceptorInitialization(
    Security::Buffer const& buffer)
{
    // Verify that we have been provided a properly formatted acceptor initialization message. 
    bool const bMatchedVerificationMessage = std::equal(
        Security::InitializationMessage.begin(), Security::InitializationMessage.end(), buffer.begin());
    if (!bMatchedVerificationMessage) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Generate random data to be used to generate the session keys. 
    auto const optPrincipalSeed = local::GenerateRandomData(PrincipalRandomLength);
    if (!optPrincipalSeed) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Add the prinicpal random data to the store in order to use it when generating session keys. 
    m_store.ExpandSessionSeed(*optPrincipalSeed);

    // Get our public key to provide the peer. 
    auto const& optPublicKey = m_store.GetPublicKey();
    if (!optPublicKey) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Make an initialization message for the acceptor containing our public key and principal 
    // random seed. 
    Security::Buffer response;
    response.reserve(optPublicKey->size() + optPrincipalSeed->size());
    response.insert(response.end(), optPublicKey->begin(), optPublicKey->end());
    response.insert(response.end(), optPrincipalSeed->begin(), optPrincipalSeed->end());

    // We expect to be provided the peer's public key and principal random seed in the next stage. 
    m_synchronization.stage = static_cast<std::uint8_t>(AcceptorSynchronizationStage::Encapsulation);

    return { Security::SynchronizationStatus::Processing, response };
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult CPQNISTL3Strategy::HandleAcceptorEncapsulation(
    Security::Buffer const& buffer)
{
    // Process the synchronization message. 
    {
        auto const& details = m_kem.get_details();
        
        // If the provided buffer size is not exactly the size of message we expect, we can not
        // proceed with the synchronization process. 
        if (std::uint32_t const expected = (details.length_public_key + PrincipalRandomLength);
            buffer.size() != expected) {
            m_synchronization.status = Security::SynchronizationStatus::Error;
            return { m_synchronization.status, {} };
        }

        // The first section of the buffer is expected to be the peer's public key. 
        m_store.SetPeerPublicKey({ buffer.begin(), buffer.begin() + details.length_public_key });

        // The second section of the buffer is expected to be random data to be used as part of 
        // session key derivation process. 
        m_store.ExpandSessionSeed({ buffer.end() - PrincipalRandomLength, buffer.end() });
    }

    // Create an encapsulated shared secret using the peer's public key. If the process fails,
    // the synchronization failed and we cannot proceed. The encapsulated secret will packaged
    // along with verification data and a signature. 
    auto optEncapsulation = EncapsulateSharedSecret();
    if (!optEncapsulation) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return {m_synchronization.status, {} };
    }

    // Get the verification data to provide as part of the message, such that the peer can verify
    // the decapsulation was successful. 
    auto const& optVerificationData = m_store.GetVerificationData();
    if (!optVerificationData) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Encrypt verification data, to both challenge the peer and so an honest peer can verify that
    // the verification data matches their own generated session keys. 
    Security::OptionalBuffer optVerificationMessage = Encrypt(
        *optVerificationData, optVerificationData->size(), 0);
    if (!optVerificationMessage) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Pack the encapsulation and verification message into a buffer the initator can parse. 
    Security::Buffer message = local::GenerateEncapsulationMessage(
        *optEncapsulation, *optVerificationMessage);

    // Sign the encapsulated secret and verification data, such that the peer can verify the message
    // after decapsulation has occured. If the message for some reason could not be signed, there 
    // is nothing else to do. 
    if(Sign(message) == 0) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // We expect to be provided the peer's verification data in the next stage.  
    m_synchronization.stage = static_cast<std::uint8_t>(AcceptorSynchronizationStage::Verification);
    return { Security::SynchronizationStatus::Processing, message };
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult CPQNISTL3Strategy::HandleAcceptorVerification(
    Security::Buffer const& buffer)
{
    // In the acceptor's verification stage we expect to have been provided the initator's 
    // signed verfiection data. If the buffer could not be verified, it is an error. 
    if (auto const status = Verify(buffer); status != Security::VerificationStatus::Success) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Get our own verification data to verify the peer's verification data aligns with our
    // own. 
    auto const& optVerificationData = m_store.GetVerificationData();
    if (!optVerificationData) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Given the buffer has been verified we can decrypt the provided verification data to 
    // ensure our the session keys align with the peer. 
    auto const optDecryptedData = Decrypt(buffer, optVerificationData->size(), 0);
    if (!optDecryptedData) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // Verify the provided verification data matches the verification data we have generated. 
    // If the data does not match, it is an error. 
    bool const bMatchedVerificationData = std::equal(
        optVerificationData->begin(), optVerificationData->end(), optDecryptedData->begin());
    if (!bMatchedVerificationData) {
        m_synchronization.status = Security::SynchronizationStatus::Error;
        return { m_synchronization.status, {} };
    }

    // The synchronization process is now complete. 
    m_synchronization.stage = static_cast<std::uint8_t>(AcceptorSynchronizationStage::Complete);
    m_synchronization.status = Security::SynchronizationStatus::Ready;

    return { Security::SynchronizationStatus::Ready, {} };
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Generate and encapsulates an ephemeral session key using NTRU-HPS-2048-677. The
// caller is provided the encapsulated shared secret to provide the peer. If an error is 
// encountered nullopt is provided instead. 
//------------------------------------------------------------------------------------------------
Security::OptionalBuffer CPQNISTL3Strategy::EncapsulateSharedSecret()
{
    // A shared secret cannot be generated and encapsulated without the peer's public key. 
    auto const& optPeerPublicKey = m_store.GetPeerPublicKey();
    if (!optPeerPublicKey) {
        return {};
    }

    // Try to generate and encapsulate a shared secret. If the OQS method throws an error,
    // return nothing to signal it failed. 
    try {
        auto [encaped, secret] = m_kem.encap_secret(*optPeerPublicKey);
        m_store.GenerateSessionKeys(
            m_role, std::move(secret), local::EncryptionKeySize, SignatureLength);
        return encaped;
    }
    catch(...) {
        return {};
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Decapsulates an ephemeral session key using NTRU-HPS-2048-677 from the provided
// encapsulated ciphertext. 
//------------------------------------------------------------------------------------------------
bool CPQNISTL3Strategy::DecapsulateSharedSecret(Security::Buffer const& encapsulation)
{
    // Try to generate and decapsulate the shared secret. If the OQS method throws an error,
    // signal that the operation did not succeed. 
    try {
        m_store.GenerateSessionKeys(
            m_role, std::move(m_kem.decap_secret(encapsulation)),
            local::EncryptionKeySize, SignatureLength);
        return true;
    }
    catch(...) {
        return false;
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Generate and return a buffer of the provided size filled with random data. 
//------------------------------------------------------------------------------------------------
Security::OptionalBuffer local::GenerateRandomData(std::uint32_t size)
{
    Security::Buffer buffer = std::vector<std::uint8_t>(size, 0x00);
    if (!RAND_bytes(buffer.data(), size) || ERR_get_error() != 0) {
        return {};
    }
    return buffer;
}

//------------------------------------------------------------------------------------------------

Security::Buffer local::GenerateEncapsulationMessage(
    Security::Buffer const& encapsulation, Security::Buffer const& data)
{
    std::uint32_t const size = 
        sizeof(std::uint32_t) + encapsulation.size() + sizeof(std::uint32_t) + data.size();

    Security::Buffer buffer; 
    buffer.reserve(size);

	PackUtils::PackChunk(buffer, encapsulation, sizeof(std::uint32_t));
	PackUtils::PackChunk(buffer, data, sizeof(std::uint32_t));

    return buffer;
}

//------------------------------------------------------------------------------------------------

bool local::ParseEncapsulationMessage(
    Security::Buffer const& buffer, Security::Buffer& encapsulation, Security::Buffer& data)
{
	Security::Buffer::const_iterator begin = buffer.begin();
	Security::Buffer::const_iterator end = buffer.end();

    std::uint32_t encapsulationSize = 0; 
    if (!PackUtils::UnpackChunk(begin, end, encapsulationSize)) {
        return false;
    }

    if (!PackUtils::UnpackChunk(begin, end, encapsulation, encapsulationSize)) {
        return false;
    }

    std::uint32_t dataSize = 0; 
    if (!PackUtils::UnpackChunk(begin, end, dataSize)) {
        return false;
    }

    if (!PackUtils::UnpackChunk(begin, end, data, dataSize)) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------
