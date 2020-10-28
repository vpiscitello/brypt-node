//------------------------------------------------------------------------------------------------
// File: NISTSecurityLevelThree.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "NISTSecurityLevelThree.hpp"
#include "../SecurityUtils.hpp"
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
#include <mutex>
#include <optional>
#include <utility>
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

Security::Strategy UnpackStrategy(
    Security::Buffer::const_iterator& begin, Security::Buffer::const_iterator const& end);

Security::OptionalBuffer GenerateRandomData(std::uint32_t size);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace Initiator {
//------------------------------------------------------------------------------------------------

Security::OptionalBuffer GenerateInitializationRequest(
    Security::PQNISTL3::CContext const& context,
    Security::CKeyStore& store,
    Security::PQNISTL3::CSynchronizationTracker& synchronization);

bool HandleInitializationResponse(
    Security::PQNISTL3::CStrategy* const pStrategy,
    Security::CKeyStore& store,
    Security::PQNISTL3::CSynchronizationTracker& synchronization,
    Security::Buffer const& request);

Security::OptionalBuffer GeneratVerificationRequest(
    Security::PQNISTL3::CStrategy* const pStrategy,
    Security::PQNISTL3::CSynchronizationTracker& synchronization);

//------------------------------------------------------------------------------------------------
} // Initiator namespace
//------------------------------------------------------------------------------------------------
namespace Acceptor {
//------------------------------------------------------------------------------------------------

using OptionalInitializationResult = std::optional<std::pair<Security::Buffer, Security::Buffer>>;

bool HandleInitializationRequest(
    Security::PQNISTL3::CContext const& context,
    Security::CKeyStore& store,
    Security::PQNISTL3::CSynchronizationTracker& synchronization,
    Security::Buffer const& request);

Security::OptionalBuffer GenerateInitializationResponse(
    Security::PQNISTL3::CStrategy* const pStrategy,
    Security::CKeyStore& store,
    Security::PQNISTL3::CSynchronizationTracker& synchronization);

bool HandleVerificationRequest(
    Security::PQNISTL3::CStrategy* const pStrategy,
    Security::PQNISTL3::CSynchronizationTracker& synchronization,
    Security::Buffer const& request);

//------------------------------------------------------------------------------------------------
} // Acceptor namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Declare the static shared context the strategy may use in a shared application context. 
//------------------------------------------------------------------------------------------------

std::shared_ptr<Security::PQNISTL3::CContext> Security::PQNISTL3::CStrategy::m_spSharedContext = nullptr;

//------------------------------------------------------------------------------------------------

Security::PQNISTL3::CContext::CContext(std::string_view kem)
    : m_kemMutex()
    , m_kem(kem.data())
    , m_publicKeyMutex()
    , m_publicKey()
{
    try {
        m_publicKey = m_kem.generate_keypair();
    }
    catch (...) {
        throw std::runtime_error("Security Context failed to generate public/private key pair!");
    }
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::PQNISTL3::CContext::GetPublicKeySize() const
{
    assert(PublicKeySize == m_kem.get_details().length_public_key);
    return PublicKeySize;
}

//------------------------------------------------------------------------------------------------

Security::Buffer Security::PQNISTL3::CContext::GetPublicKey() const
{
    return m_publicKey;
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::PQNISTL3::CContext::GetPublicKey(Buffer& buffer) const
{
    std::shared_lock lock(m_publicKeyMutex);
    buffer.insert(buffer.end(), m_publicKey.begin(), m_publicKey.end());
    return m_publicKey.size();
}

//------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::CContext::GenerateEncapsulatedSecret(
    Buffer const& publicKey, EncapsulationCallback const& callback) const
{
    std::shared_lock lock(m_kemMutex);
    try {
        auto [encaped, secret] = m_kem.encap_secret(publicKey);
        assert(encaped.size() == EncapsulationSize);
        callback(std::move(encaped), std::move(secret));
        return true;
    }
    catch(...) {
        return false;
    }
}

//------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::CContext::DecapsulateSecret(
    Buffer const& encapsulation, Buffer& decapsulation) const
{
    // Try to generate and decapsulate the shared secret. If the OQS method throws an error,
    // signal that the operation did not succeed. 
    try {
        std::shared_lock lock(m_kemMutex);
        decapsulation = std::move(m_kem.decap_secret(encapsulation));
        return true;
    }
    catch(...) {
        return false;
    }
}

//------------------------------------------------------------------------------------------------

Security::PQNISTL3::CSynchronizationTracker::CSynchronizationTracker()
    : m_status(SynchronizationStatus::Processing)
    , m_stage(0)
    , m_transaction()
    , m_signator()
    , m_verifier()
{
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationStatus Security::PQNISTL3::CSynchronizationTracker::GetStatus() const
{
    return m_status;
}

//------------------------------------------------------------------------------------------------

void Security::PQNISTL3::CSynchronizationTracker::SetError()
{
    m_status = SynchronizationStatus::Error;
}

//------------------------------------------------------------------------------------------------

template <typename EnumType>
EnumType Security::PQNISTL3::CSynchronizationTracker::GetStage() const
{
    return static_cast<EnumType>(m_stage);
}

//------------------------------------------------------------------------------------------------

template <typename EnumType>
void Security::PQNISTL3::CSynchronizationTracker::SetStage(EnumType type)
{
    assert(sizeof(type) == sizeof(m_stage));
    m_stage = static_cast<decltype(m_stage)>(type);
}

//------------------------------------------------------------------------------------------------

void Security::PQNISTL3::CSynchronizationTracker::SetSignator(TransactionSignator const& signator)
{
    m_signator = signator;
}

//------------------------------------------------------------------------------------------------

void Security::PQNISTL3::CSynchronizationTracker::SetVerifier(TransactionVerifier const& verifier)
{
    m_verifier = verifier;
}

//------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::CSynchronizationTracker::UpdateTransaction(Buffer const& buffer)
{
    m_transaction.insert(m_transaction.end(), buffer.begin(), buffer.end());
    return true;
}

//------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::CSynchronizationTracker::SignTransaction(Buffer& message)
{
    assert(m_signator); // The strategy should always have provided us a signator. 
    UpdateTransaction(message); // The transaction needs to be updated with the current message. 
    if (m_signator(m_transaction, message) == 0) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------

Security::VerificationStatus Security::PQNISTL3::CSynchronizationTracker::VerifyTransaction()
{
    assert(m_verifier); // The strategy should always have provided us a verifier. 
    return m_verifier(m_transaction);
}

//------------------------------------------------------------------------------------------------

template<typename EnumType>
void Security::PQNISTL3::CSynchronizationTracker::Finalize(EnumType stage)
{
    m_status = SynchronizationStatus::Ready;
    m_transaction.clear();
    SetStage(stage);
}

//------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::CSynchronizationTracker::ResetState()
{
    m_status = SynchronizationStatus::Processing;
    m_stage = 0;
    m_transaction.clear();
    return true;
}

//------------------------------------------------------------------------------------------------

Security::PQNISTL3::CStrategy::CStrategy(Role role, Context context)
    : m_role(role)
    , m_synchronization()
    , m_spSessionContext()
    , m_kem(KeyEncapsulationSchme.data())
    , m_store()
{
    switch (context) {
        case Context::Unique: {
            m_spSessionContext = std::make_shared<CContext>(KeyEncapsulationSchme);
        } break;
        case Context::Application: {
            if (!m_spSharedContext) {
                throw std::runtime_error("Shared Application Context has not been initialized!");
            }
            m_spSessionContext = m_spSharedContext;
        } break;
        default: assert(false); break;
    }

    m_synchronization.SetSignator(
        [this] (Buffer const& transaction, Buffer& message) -> bool
        {
            return Sign(transaction, message);
        });

    m_synchronization.SetVerifier(
        [this] (Buffer const& transaction) -> VerificationStatus
        {
            return Verify(transaction);
        });
}

//------------------------------------------------------------------------------------------------

Security::Strategy Security::PQNISTL3::CStrategy::GetStrategyType() const
{
    return Type;
}

//------------------------------------------------------------------------------------------------

Security::Role Security::PQNISTL3::CStrategy::GetRole() const
{
    return m_role;
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::PQNISTL3::CStrategy::GetSynchronizationStages() const
{
    switch (m_role) {
        case Security::Role::Initiator: return InitiatorStages;
        case Security::Role::Acceptor: return AcceptorStages;
        default: assert(false); return 0;
    }
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationStatus Security::PQNISTL3::CStrategy::GetSynchronizationStatus() const
{
    return m_synchronization.GetStatus();
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::CStrategy::PrepareSynchronization()
{
    // Reset the state of any previous synchronizations. 
    if (!m_synchronization.ResetState()) {
        return { SynchronizationStatus::Error, {} };
    }

    // If a prior synchronization was completed, clear the keys. 
    if (m_store.HasGeneratedKeys()) {
        m_store.ResetState();
    }

    // The synchronization preperation is dependent on the strategy's role. 
    switch (m_role) {
        case Role::Initiator: {
            // Generate the synchronization request with our context and seed. 
            OptionalBuffer optRequest = Initiator::GenerateInitializationRequest(
                *m_spSessionContext, m_store, m_synchronization);

            if (!optRequest) {
                m_synchronization.SetError();
                return { m_synchronization.GetStatus(), {} };
            }

            return { m_synchronization.GetStatus(), *optRequest };
        }
        case Role::Acceptor: {
            // There is no initialization messages needed from the acceptor strategy. 
            return { m_synchronization.GetStatus(), {} };
        }
        default: assert(false); return { SynchronizationStatus::Error, {} }; // What is this?
    }
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::CStrategy::Synchronize(Buffer const& buffer)
{
    switch (m_role) {
        case Role::Initiator: return HandleInitiatorSynchronization(buffer);
        case Role::Acceptor: return HandleAcceptorSynchronization(buffer);
        default: assert(false); break; // What is this?
    }
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PQNISTL3::CStrategy::Encrypt(
    Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const
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
	if (ERR_get_error() != 0 || !upCipherContext) {
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
	if (!EVP_EncryptInit_ex(upCipherContext.get(), EVP_aes_256_ctr(), nullptr, pKey, iv.data())) {
		return {};
	}

    // Sanity check that our encryption key and IV are the size expected by OpenSSL. 
    assert(std::int32_t(keySize) == EVP_CIPHER_CTX_key_length(upCipherContext.get()));
    assert(std::int32_t(iv.size()) == EVP_CIPHER_CTX_iv_length(upCipherContext.get()));

	Buffer ciphertext(size, 0x00); // Create a buffer to store the encrypted data. 
	auto pCiphertext = reinterpret_cast<std::uint8_t*>(ciphertext.data());
	auto const pPlaintext = reinterpret_cast<std::uint8_t const*>(buffer.data());

    // Encrypt the plaintext into the ciphertext buffer. 
	std::int32_t encrypted = 0;
	if (!EVP_EncryptUpdate(upCipherContext.get(), pCiphertext, &encrypted, pPlaintext, size) || encrypted == 0) {
		return {};
	}

    // Cleanup the OpenSSL encryption cipher. 
	if (!EVP_EncryptFinal_ex(upCipherContext.get(), pCiphertext + encrypted, &encrypted)) {
		return {};
	}

	return ciphertext;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PQNISTL3::CStrategy::Decrypt(
    Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const
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
	if (ERR_get_error() != 0 || !upCipherContext) {
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
	if (!EVP_DecryptInit_ex(upCipherContext.get(), EVP_aes_256_ctr(), nullptr, pKey, iv.data())) {
		return {};
	}

    // Sanity check that our decryption key and IV are the size expected by OpenSSL.
    assert(std::int32_t(keySize) == EVP_CIPHER_CTX_key_length(upCipherContext.get()));
    assert(std::int32_t(iv.size()) == EVP_CIPHER_CTX_iv_length(upCipherContext.get()));

	Buffer plaintext(size, 0x00); // Create a buffer to store the decrypted data. 
	auto pPlaintext = reinterpret_cast<std::uint8_t*>(plaintext.data());
	auto const pCiphertext = reinterpret_cast<std::uint8_t const*>(buffer.data());

    // Decrypt the ciphertext into the plaintext buffer.
	std::int32_t decrypted = 0;
	if (!EVP_DecryptUpdate(upCipherContext.get(), pPlaintext, &decrypted, pCiphertext, size) || decrypted == 0) {
		return {};
	}

    // Cleanup the OpenSSL decryption cipher.
	if (!EVP_DecryptFinal_ex(upCipherContext.get(), pPlaintext + decrypted ,&decrypted)) {
		return {};
	}

	return plaintext;
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::PQNISTL3::CStrategy::Sign(Buffer& buffer) const
{
    return Sign(buffer, buffer); // Generate and add the signature to the provided buffer. 
}

//------------------------------------------------------------------------------------------------

Security::VerificationStatus Security::PQNISTL3::CStrategy::Verify(Buffer const& buffer) const
{
    // Ensure the caller is able to verify the buffer with generated session keys.
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot decrypt before synchronization is complete!");
    }

    // Determine the amount of non-signature data is packed into the buffer. 
    std::int64_t packContentSize = buffer.size() - SignatureSize;
	if (buffer.empty() || packContentSize <= 0) {
		return VerificationStatus::Failed;
	}

    // Get the peer's signature key to be used to generate the expected siganture..
    auto const optPeerSignatureKey = m_store.GetPeerSignatureKey();
    if (!optPeerSignatureKey) {
		return VerificationStatus::Failed;
    }

    // Destructure the KeyStore result into meaniful names. 
    auto const [pKey, keySize] = *optPeerSignatureKey;
    assert(keySize == SignatureSize);

    // Create the signature that peer should have provided. 
	auto const optGeneratedBuffer = GenerateSignature(pKey, keySize, buffer.data(), packContentSize);
	if (!optGeneratedBuffer) {
		return VerificationStatus::Failed;
	}

    // Compare the generated signature with the signature attached to the buffer. 
	auto const result = CRYPTO_memcmp(
		optGeneratedBuffer->data(), buffer.data() + packContentSize, optGeneratedBuffer->size());

    // If the signatures are not equal than the peer did not sign the buffer or the buffer was
    // altered in transmission. 
	if (result != 0) {
		return VerificationStatus::Failed;
	}

	return VerificationStatus::Success;
}

//------------------------------------------------------------------------------------------------

void Security::PQNISTL3::CStrategy::InitializeApplicationContext()
{
    CStrategy::m_spSharedContext = std::make_shared<CContext>(
        KeyEncapsulationSchme);
}

//------------------------------------------------------------------------------------------------

void Security::PQNISTL3::CStrategy::ShutdownApplicationContext()
{
    CStrategy::m_spSharedContext.reset();
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<Security::PQNISTL3::CContext> Security::PQNISTL3::CStrategy::GetSessionContext() const
{
    return m_spSessionContext;
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::PQNISTL3::CStrategy::GetPublicKeySize() const
{
    return m_spSessionContext->GetPublicKeySize();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Generate and encapsulates an ephemeral session key using NTRU-HPS-2048-677. The
// caller is provided the encapsulated shared secret to provide the peer. If an error is 
// encountered nullopt is provided instead. 
//------------------------------------------------------------------------------------------------
Security::OptionalBuffer Security::PQNISTL3::CStrategy::EncapsulateSharedSecret()
{
    // A shared secret cannot be generated and encapsulated without the peer's public key. 
    auto const& optPeerPublicKey = m_store.GetPeerPublicKey();
    if (!optPeerPublicKey) {
        return {};
    }

    // The session context should always exist after the constructor is called. 
    assert(m_spSessionContext);

    Security::Buffer encapsulation;
    // Use the session context to generate a secret for using the peer's public key. 
    bool const success = m_spSessionContext->GenerateEncapsulatedSecret(
        *optPeerPublicKey,
        [this, &encapsulation] (Buffer&& encaped, Buffer&& secret)
        {
            m_store.GenerateSessionKeys(
                m_role, std::move(secret), local::EncryptionKeySize, SignatureSize);

            encapsulation = std::move(encaped);
        });

    if (!success) {
        return {};
    }

    return encapsulation;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Decapsulates an ephemeral session key using NTRU-HPS-2048-677 from the provided
// encapsulated ciphertext. 
//------------------------------------------------------------------------------------------------
bool Security::PQNISTL3::CStrategy::DecapsulateSharedSecret(Buffer const& encapsulation)
{
    // The session context should always exist after the constructor is called. 
    assert(m_spSessionContext);

    Buffer decapsulation;
    if (!m_spSessionContext->DecapsulateSecret(encapsulation, decapsulation)) {
        return false;
    }
    
    return m_store.GenerateSessionKeys(
        m_role, std::move(decapsulation), local::EncryptionKeySize, SignatureSize);
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PQNISTL3::CStrategy::GenerateVerficationData()
{
    assert(m_store.HasGeneratedKeys());

    // Get the verification data to provide to encrypt.
    auto const& optData = m_store.GetVerificationData();
    if (!optData) {
        m_synchronization.SetError();
        return {};
    }

    // Encrypt verification data to challenge the peer's keys. 
    Security::OptionalBuffer optEncrypted = Encrypt(*optData, optData->size(), 0);
    if (!optEncrypted) {
        m_synchronization.SetError();
        return {};
    }

    // Provided the encrypted verification data to the caller. 
    return optEncrypted;
}

//------------------------------------------------------------------------------------------------

Security::VerificationStatus Security::PQNISTL3::CStrategy::VerifyKeyShare(Buffer const& buffer) const
{
    // Get our own verification data to verify the provided encrypted data. 
    auto const& optVerificationData = m_store.GetVerificationData();
    if (!optVerificationData) {
        return VerificationStatus::Failed;
    }

    // Decrypt the provided data to get the peer's verification data. 
    auto const optDecryptedData = Decrypt(buffer, buffer.size(), 0);
    if (!optDecryptedData) {
        return VerificationStatus::Failed;
    }

    // Verify the provided verification data matches the verification data we have generated. 
    // If the data does not match, it is an error. 
    bool const bMatchedVerificationData = std::equal(
        optVerificationData->begin(), optVerificationData->end(), optDecryptedData->begin());
    if (!bMatchedVerificationData) {
        return VerificationStatus::Failed;
    }

    return VerificationStatus::Success;
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::PQNISTL3::CStrategy::Sign(
    Security::Buffer const& source, Security::Buffer& destination) const
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
    assert(keySize == SignatureSize);

    // Sign the provided buffer with our signature key .
    OptionalBuffer optSignature = GenerateSignature(pKey, keySize, source.data(), source.size());
    if (!optSignature) {
        return 0;
    }

    // Insert the signature to create a verifiable buffer. 
    destination.insert(destination.end(), optSignature->begin(), optSignature->end());
    return optSignature->size(); // Tell the caller how many bytes have been inserted. 
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PQNISTL3::CStrategy::GenerateSignature(
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
    return Buffer(&signature[0], &signature[hashed]);
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::CStrategy::HandleInitiatorSynchronization(
    Buffer const& buffer)
{
    switch (m_synchronization.GetStage<InitiatorStage>())
    {
        case InitiatorStage::Initialization: {
            return HandleInitiatorInitialization(buffer);
        }
        // It is an error to be called in all other synchrinization stages. 
        case InitiatorStage::Complete: {
            m_synchronization.SetError();
            return { m_synchronization.GetStatus(), {} };
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::CStrategy::HandleInitiatorInitialization(
    Buffer const& buffer)
{
    // Handle the acceptor's resposne to the initialization message. The post conditions for 
    // this handling include generating the session keys and verifying the plaintext transaction. 
    bool const success = Initiator::HandleInitializationResponse(
        this, m_store, m_synchronization, buffer);

    // Set an error if the response could not be handled. 
    if (!success) {
        m_synchronization.SetError();
        return { m_synchronization.GetStatus(), {} };
    }

    // As a response we need to provide a verification request for the acceptor. After this point,
    // the synchronization will have been finalized.  
    auto const optRequest = Initiator::GeneratVerificationRequest(this, m_synchronization);

    // Set an error if the request could not be generated. 
    if (!optRequest) {
        m_synchronization.SetError();
        return { m_synchronization.GetStatus(), {} };
    }

    return { m_synchronization.GetStatus(), *optRequest };
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::CStrategy::HandleAcceptorSynchronization(
    Buffer const& buffer)
{
    switch (m_synchronization.GetStage<AcceptorStage>())
    {
        case AcceptorStage::Initialization: {
            return HandleAcceptorInitialization(buffer);
        }
        case AcceptorStage::Verification: {
            return HandleAcceptorVerification(buffer);
        }
        // It is an error to be called in all other synchrinization stages. 
        case AcceptorStage::Complete: {
            m_synchronization.SetError();
            return { m_synchronization.GetStatus(), {} };
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::CStrategy::HandleAcceptorInitialization(
    Buffer const& buffer)
{
    // The session context should always exist after the constructor is called. 
    assert(m_spSessionContext);

    // Handle the initiators's initialization request. The post conditions for this handling 
    // include generating the session keys and updating the transaction's plaintext data. 
    auto const success = Acceptor::HandleInitializationRequest(
        *m_spSessionContext, m_store, m_synchronization, buffer);

    // Set an error if the request could not be handled. 
    if (!success) {
        m_synchronization.SetError();
        return { m_synchronization.GetStatus(), {} };
    }

    // Generate the response to the initialization request. This response will include our 
    // random seed for the key generation, an encapsulated shared secret, verification data,
    // and the transaction's signature. 
    auto const optResponse = Acceptor::GenerateInitializationResponse(
        this, m_store, m_synchronization);

    // Set an error if the response could not be generated. 
    if (!optResponse) {
        m_synchronization.SetError();
        return { m_synchronization.GetStatus(), {} };
    }

    return { m_synchronization.GetStatus(), *optResponse };
}

//------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::CStrategy::HandleAcceptorVerification(
    Buffer const& buffer)
{
    // Handle the initiator's verification request. As post condition the synchronization will
    // be finalized. If the verfication data could not be verified set an error. 
    if (!Acceptor::HandleVerificationRequest(this, m_synchronization, buffer)) {
        m_synchronization.SetError();
    }

    // There are no further synchronization messages to be provided. 
    return { m_synchronization.GetStatus(), {} };
}

//------------------------------------------------------------------------------------------------

Security::Strategy local::UnpackStrategy(
    Security::Buffer::const_iterator& begin, Security::Buffer::const_iterator const& end)
{
    using StrategyType = std::underlying_type_t<Security::Strategy>;

    StrategyType strategy = 0;
    PackUtils::UnpackChunk(begin, end, strategy);
    return Security::ConvertToStrategy(strategy);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Generate and return a buffer of the provided size filled with random data. 
//------------------------------------------------------------------------------------------------
Security::OptionalBuffer local::GenerateRandomData(std::uint32_t size)
{
    Security::Buffer buffer = std::vector<std::uint8_t>(size, 0x00);
    if (!RAND_bytes(buffer.data(), size)) {
        return {};
    }
    return buffer;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Initiator::GenerateInitializationRequest(
    Security::PQNISTL3::CContext const& context,
    Security::CKeyStore& store,
    Security::PQNISTL3::CSynchronizationTracker& synchronization)
{
    using namespace Security::PQNISTL3;

    // Generate our prinicpal seed for the session. 
    auto const optPrincipalSeed = local::GenerateRandomData(CStrategy::PrincipalRandomSize);
    if (!optPrincipalSeed) {
        return {};
    }

    // Expand the KeyStore's deriviation seed. 
    store.ExpandSessionSeed(*optPrincipalSeed);

    constexpr std::uint32_t const size = 
        sizeof(CStrategy::Type) + CStrategy::PrincipalRandomSize + CContext::PublicKeySize;

    Security::Buffer request;
    request.reserve(size);

    PackUtils::PackChunk(request, CStrategy::Type); // Pack the strategy type.
    PackUtils::PackChunk(request, *optPrincipalSeed);
    if (std::uint32_t fetched = context.GetPublicKey(request); fetched == 0) {
        return {};
    }

    // Update the synchronization's plaintext verification buffer with the request
    synchronization.UpdateTransaction(request);

    return request;
}

//------------------------------------------------------------------------------------------------

bool Initiator::HandleInitializationResponse(
    Security::PQNISTL3::CStrategy* const pStrategy,
    Security::CKeyStore& store,
    Security::PQNISTL3::CSynchronizationTracker& synchronization,
    Security::Buffer const& request)
{
    using namespace Security::PQNISTL3;

    assert(pStrategy);

	Security::Buffer::const_iterator begin = request.begin();
	Security::Buffer::const_iterator end = request.end();

    Security::Strategy strategy = local::UnpackStrategy(begin, end);
    if (strategy != CStrategy::Type) {
        return {};
    }

    constexpr std::uint32_t const expected = 
        sizeof(CStrategy::Type) + CStrategy::PrincipalRandomSize + CContext::EncapsulationSize +
        Security::CKeyStore::VerificationSize + CStrategy::SignatureSize;
    if (request.size() != expected) {
        return false;
    }

    // Handle the peer's packed prinicpal random seed. 
    {
        Security::Buffer seed;
        seed.reserve(CStrategy::PrincipalRandomSize);

        if (!PackUtils::UnpackChunk(begin, end, seed, CStrategy::PrincipalRandomSize)) {
            return false;
        }

        // Expand the key store's deriviation seed with the provided data. 
        store.ExpandSessionSeed(seed);
    }

    // Handle the peer's packed encapsulation data. 
    {
        Security::Buffer encapsulation;
        encapsulation.reserve(CContext::EncapsulationSize);

        if (!PackUtils::UnpackChunk(begin, end, encapsulation, CContext::EncapsulationSize)) {
            return false;
        }

        // Attempt to decapsulate the shared secret. If the shared secret could not be decapsulated
        // or the session keys fail to be generated return an error. 
        if (!pStrategy->DecapsulateSharedSecret(encapsulation)) {
            return false;
        }
    }

    // Handle the peer's verification data.
    {
        Security::Buffer verification;
        verification.reserve(Security::CKeyStore::VerificationSize);

        if (!PackUtils::UnpackChunk(begin, end, verification, Security::CKeyStore::VerificationSize)) {
            return false;
        }

        if (pStrategy->VerifyKeyShare(verification) != Security::VerificationStatus::Success) {
            return false;
        }
    }

    // Add the acceptor's response data to the transaction and verify the unauthenticated 
    // synchronization stages. 
    {
        // Add the request to the synchronization transaction. 
        synchronization.UpdateTransaction(request);

        // The request will have an attached transaction signature that must be verified. 
        if (synchronization.VerifyTransaction() != Security::VerificationStatus::Success) {
            return false;
        }
    }

    return true;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Initiator::GeneratVerificationRequest(
    Security::PQNISTL3::CStrategy* const pStrategy,
    Security::PQNISTL3::CSynchronizationTracker& synchronization)
{
    using namespace Security::PQNISTL3;

    assert(pStrategy);

    auto const& optVerification = pStrategy->GenerateVerficationData();
    if (!optVerification) {
        return {};
    }

    constexpr std::uint32_t const size = 
        sizeof(CStrategy::Type) + Security::CKeyStore::VerificationSize +
        CStrategy::SignatureSize;

    Security::Buffer request;
    request.reserve(size);

    PackUtils::PackChunk(request, CStrategy::Type);
    PackUtils::PackChunk(request, *optVerification);

    // If for some reason we cannot sign the verification data it is an error. 
    if (pStrategy->Sign(request) == 0) {
        return {};
    }

    // The synchronization process is now complete.
    synchronization.Finalize(CStrategy::InitiatorStage::Complete);

    return request;
}

//------------------------------------------------------------------------------------------------

bool Acceptor::HandleInitializationRequest(
    Security::PQNISTL3::CContext const& context,
    Security::CKeyStore& store,
    Security::PQNISTL3::CSynchronizationTracker& synchronization,
    Security::Buffer const& request)
{
    using namespace Security::PQNISTL3;

	Security::Buffer::const_iterator begin = request.begin();
	Security::Buffer::const_iterator end = request.end();

    Security::Strategy strategy = local::UnpackStrategy(begin, end);
    if (strategy != CStrategy::Type) {
        return false;
    }

    constexpr std::uint32_t const expected = 
        sizeof(CStrategy::Type) + CStrategy::PrincipalRandomSize + CContext::PublicKeySize;
    if (request.size() != expected) {
        return false;
    }

    // Handle the peer's packed prinicpal random seed. 
    {
        Security::Buffer seed;
        seed.reserve(CStrategy::PrincipalRandomSize);

        if (!PackUtils::UnpackChunk(begin, end, seed, CStrategy::PrincipalRandomSize)) {
            return false;
        }

        // Expand the key store's deriviation seed with the provided data. 
        store.ExpandSessionSeed(seed);
    }

    // Handle the peer's packed publick key.
    {
        Security::Buffer key;
        std::uint32_t const size = context.GetPublicKeySize();
        key.reserve(size);

        if (!PackUtils::UnpackChunk(begin, end, key, size)) {
            return false;
        }
        
        store.SetPeerPublicKey(std::move(key)); // Store the peer's public key.
    }

    // Add the request to the synchronization transaction. 
    synchronization.UpdateTransaction(request);

    return true;
}

//------------------------------------------------------------------------------------------------

Security::OptionalBuffer Acceptor::GenerateInitializationResponse(
    Security::PQNISTL3::CStrategy* const pStrategy,
    Security::CKeyStore& store,
    Security::PQNISTL3::CSynchronizationTracker& synchronization)
{
    using namespace Security::PQNISTL3;

    assert(pStrategy);

    // Generate random data to be used to generate the session keys. 
    auto const optPrincipalSeed = local::GenerateRandomData(CStrategy::PrincipalRandomSize);
    if (!optPrincipalSeed) {
        return {};
    }

    // Add the prinicpal random data to the store in order to use it when generating session keys. 
    store.ExpandSessionSeed(*optPrincipalSeed);

    // Create an encapsulated shared secret using the peer's public key. If the process fails,
    // the synchronization failed and we cannot proceed. 
    auto optEncapsulation = pStrategy->EncapsulateSharedSecret();
    if (!optEncapsulation) {
        return {};
    }

    // Generate the verififcation data needed for the response. If the process fails, return an error. 
    auto optVerification = pStrategy->GenerateVerficationData();
    if (!optVerification) {
        return {};
    }

    constexpr std::uint32_t const size = 
        sizeof(CStrategy::Type) + CStrategy::PrincipalRandomSize + CContext::EncapsulationSize +
        Security::CKeyStore::VerificationSize + CStrategy::SignatureSize;

    Security::Buffer response; 
    response.reserve(size);

    PackUtils::PackChunk(response, CStrategy::Type);
    PackUtils::PackChunk(response, *optPrincipalSeed);
	PackUtils::PackChunk(response, *optEncapsulation);
	PackUtils::PackChunk(response, *optVerification);

    synchronization.SetStage(CStrategy::AcceptorStage::Verification);
    if (!synchronization.SignTransaction(response)) {
        return {};
    }

    return response;
}

//------------------------------------------------------------------------------------------------

bool Acceptor::HandleVerificationRequest(
    Security::PQNISTL3::CStrategy* const pStrategy,
    Security::PQNISTL3::CSynchronizationTracker& synchronization,
    Security::Buffer const& request)
{
    using namespace Security::PQNISTL3;

    assert(pStrategy);

    // In the acceptor's verification stage we expect to have been provided the initator's 
    // signed verfiection data. If the buffer could not be verified, it is an error. 
    if (pStrategy->Verify(request) != Security::VerificationStatus::Success) {
        return false;
    }

	Security::Buffer::const_iterator begin = request.begin();
	Security::Buffer::const_iterator end = request.end();

    Security::Strategy strategy = local::UnpackStrategy(begin, end);
    if (strategy != CStrategy::Type) {
        return {};
    }

    constexpr std::uint32_t const expected = 
        sizeof(CStrategy::Type) + Security::CKeyStore::VerificationSize +
        CStrategy::SignatureSize;
    if (request.size() != expected) {
        return false;
    }

    // Handle the peer's verification data.
    {
        Security::Buffer verification;
        verification.reserve(Security::CKeyStore::VerificationSize);

        if (!PackUtils::UnpackChunk(begin, end, verification, Security::CKeyStore::VerificationSize)) {
            return false;
        }

        // Verify the packed and encrypted verification data. 
        if (pStrategy->VerifyKeyShare(verification) != Security::VerificationStatus::Success) {
            return false;
        }
    }

    // The synchronization process is now complete.
    synchronization.Finalize(CStrategy::AcceptorStage::Complete);

    return true;
}

//------------------------------------------------------------------------------------------------
