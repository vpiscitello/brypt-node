//----------------------------------------------------------------------------------------------------------------------
// File: NISTSecurityLevelThree.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "NISTSecurityLevelThree.hpp"
#include "BryptMessage/PackUtils.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/conf.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
//----------------------------------------------------------------------------------------------------------------------
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

// Various size constants required for AES-256-CTR 
constexpr std::size_t EncryptionKeySize = 32; // In bytes, 256 bits. 
constexpr std::size_t EncryptionIVSize = 16; // In bytes, 128 bits. 
constexpr std::size_t EncryptionBlockSize = 16; // In bytes, 128 bits. 

using CipherContext = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

[[nodiscard]] Security::Strategy UnpackStrategy(
    Security::ReadableView::iterator& begin, Security::ReadableView::iterator const& end);

[[nodiscard]] Security::OptionalBuffer GenerateRandomData(std::size_t size);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace Initiator {
//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] Security::OptionalBuffer GenerateInitializationRequest(
    Security::PQNISTL3::Context const& context,
    Security::KeyStore& store,
    Security::PQNISTL3::SynchronizationTracker& synchronization);

[[nodiscard]] bool HandleInitializationResponse(
    Security::PQNISTL3::Strategy* const pStrategy,
    Security::KeyStore& store,
    Security::PQNISTL3::SynchronizationTracker& synchronization,
    Security::ReadableView request);

[[nodiscard]] Security::OptionalBuffer GeneratVerificationRequest(
    Security::PQNISTL3::Strategy* const pStrategy,
    Security::PQNISTL3::SynchronizationTracker& synchronization);

//----------------------------------------------------------------------------------------------------------------------
} // Initiator namespace
//----------------------------------------------------------------------------------------------------------------------
namespace Acceptor {
//----------------------------------------------------------------------------------------------------------------------

using OptionalInitializationResult = std::optional<std::pair<Security::Buffer, Security::Buffer>>;

[[nodiscard]] bool HandleInitializationRequest(
    Security::PQNISTL3::Context const& context,
    Security::KeyStore& store,
    Security::PQNISTL3::SynchronizationTracker& synchronization,
    Security::ReadableView request);

[[nodiscard]] Security::OptionalBuffer GenerateInitializationResponse(
    Security::PQNISTL3::Strategy* const pStrategy,
    Security::KeyStore& store,
    Security::PQNISTL3::SynchronizationTracker& synchronization);

[[nodiscard]] bool HandleVerificationRequest(
    Security::PQNISTL3::Strategy* const pStrategy,
    Security::PQNISTL3::SynchronizationTracker& synchronization,
    Security::ReadableView request);

//----------------------------------------------------------------------------------------------------------------------
} // Acceptor namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Declare the static shared context the strategy may use in a shared application context. 
//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Security::PQNISTL3::Context> Security::PQNISTL3::Strategy::m_spSharedContext = nullptr;

//----------------------------------------------------------------------------------------------------------------------

Security::PQNISTL3::Context::Context(std::string_view kem)
    : m_kemMutex()
    , m_kem(kem.data())
    , m_publicKeyMutex()
    , m_publicKey()
{
    try {
        m_publicKey = m_kem.generate_keypair();
    } catch (...) {
        throw std::runtime_error("Security Context failed to generate public/private key pair!");
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::PQNISTL3::Context::GetPublicKeySize() const
{
    assert(PublicKeySize == m_kem.get_details().length_public_key);
    return PublicKeySize;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Buffer Security::PQNISTL3::Context::GetPublicKey() const
{
    return m_publicKey;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::PQNISTL3::Context::GetPublicKey(Buffer& buffer) const
{
    std::shared_lock lock(m_publicKeyMutex);
    buffer.insert(buffer.end(), m_publicKey.begin(), m_publicKey.end());
    return m_publicKey.size();
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::Context::GenerateEncapsulatedSecret(
    Buffer const& publicKey, EncapsulationCallback const& callback) const
{
    std::shared_lock lock(m_kemMutex);
    try {
        auto [encaped, secret] = m_kem.encap_secret(publicKey);
        assert(encaped.size() == EncapsulationSize);
        return callback(std::move(encaped), std::move(secret));
    } catch(...) {
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::Context::DecapsulateSecret(
    Buffer const& encapsulation, Buffer& decapsulation) const
{
    // Try to generate and decapsulate the shared secret. If the OQS method throws an error,
    // signal that the operation did not succeed. 
    try {
        std::shared_lock lock(m_kemMutex);
        decapsulation = std::move(m_kem.decap_secret(encapsulation));
        return true;
    } catch(...) {
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Security::PQNISTL3::SynchronizationTracker::SynchronizationTracker()
    : m_status(SynchronizationStatus::Processing)
    , m_stage(0)
    , m_transaction()
    , m_signator()
    , m_verifier()
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationStatus Security::PQNISTL3::SynchronizationTracker::GetStatus() const
{
    return m_status;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::PQNISTL3::SynchronizationTracker::SetError()
{
    m_status = SynchronizationStatus::Error;
}

//----------------------------------------------------------------------------------------------------------------------

template <typename EnumType>
EnumType Security::PQNISTL3::SynchronizationTracker::GetStage() const
{
    return static_cast<EnumType>(m_stage);
}

//----------------------------------------------------------------------------------------------------------------------

template <typename EnumType>
void Security::PQNISTL3::SynchronizationTracker::SetStage(EnumType type)
{
    assert(sizeof(type) == sizeof(m_stage));
    m_stage = static_cast<decltype(m_stage)>(type);
}

//----------------------------------------------------------------------------------------------------------------------

void Security::PQNISTL3::SynchronizationTracker::SetSignator(TransactionSignator const& signator)
{
    m_signator = signator;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::PQNISTL3::SynchronizationTracker::SetVerifier(TransactionVerifier const& verifier)
{
    m_verifier = verifier;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::PQNISTL3::SynchronizationTracker::UpdateTransaction(ReadableView buffer)
{
    m_transaction.insert(m_transaction.end(), buffer.begin(), buffer.end());
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::SynchronizationTracker::SignTransaction(Buffer& message)
{
    assert(m_signator); // The strategy should always have provided us a signator. 
    UpdateTransaction(message); // The transaction needs to be updated with the current message. 

    if (m_signator(m_transaction, message) == 0) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Security::VerificationStatus Security::PQNISTL3::SynchronizationTracker::VerifyTransaction()
{
    assert(m_verifier); // The strategy should always have provided us a verifier. 
    return m_verifier(m_transaction);
}

//----------------------------------------------------------------------------------------------------------------------

template<typename EnumType>
void Security::PQNISTL3::SynchronizationTracker::Finalize(EnumType stage)
{
    m_status = SynchronizationStatus::Ready;
    m_transaction.clear();
    SetStage(stage);
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::PQNISTL3::SynchronizationTracker::ResetState()
{
    m_status = SynchronizationStatus::Processing;
    m_stage = 0;
    m_transaction.clear();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Security::PQNISTL3::Strategy::Strategy(Role role, Security::Context context)
    : m_role(role)
    , m_context(context)
    , m_spSessionContext()
    , m_synchronization()
    , m_store()
{
    switch (context) {
        case Security::Context::Unique: {
            m_spSessionContext = std::make_shared<Context>(KeyEncapsulationScheme);
        } break;
        case Security::Context::Application: {
            if (!m_spSharedContext) [[unlikely]] {
                throw std::runtime_error("Application security context has not been initialized!");
            }
            m_spSessionContext = m_spSharedContext;
        } break;
        default: assert(false); break;
    }

    m_synchronization.SetSignator(
        [this] (auto const& transaction, auto& message) -> bool
        { return Sign(transaction, message); });

    m_synchronization.SetVerifier(
        [this] (auto const& transaction) -> VerificationStatus
        { return Verify(transaction); });
}

//----------------------------------------------------------------------------------------------------------------------

Security::Strategy Security::PQNISTL3::Strategy::GetStrategyType() const
{
    return Type;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Role Security::PQNISTL3::Strategy::GetRoleType() const
{
    return m_role;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Context Security::PQNISTL3::Strategy::GetContextType() const
{
    return m_context;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::PQNISTL3::Strategy::GetSignatureSize() const
{
    return SignatureSize;
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Security::PQNISTL3::Strategy::GetSynchronizationStages() const
{
    switch (m_role) {
        case Security::Role::Initiator: return InitiatorStages;
        case Security::Role::Acceptor: return AcceptorStages;
        default: assert(false); return 0;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationStatus Security::PQNISTL3::Strategy::GetSynchronizationStatus() const
{
    return m_synchronization.GetStatus();
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::Strategy::PrepareSynchronization()
{
    // Reset the state of any previous synchronizations. 
    if (!m_synchronization.ResetState()) { return { SynchronizationStatus::Error, {} }; }

    // If a prior synchronization was completed, clear the keys. 
    if (m_store.HasGeneratedKeys()) { m_store.ResetState(); }

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

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::Strategy::Synchronize(ReadableView buffer)
{
    switch (m_role) {
        case Role::Initiator: return HandleInitiatorSynchronization(buffer);
        case Role::Acceptor: return HandleAcceptorSynchronization(buffer);
        default: assert(false); break; // What is this?
    }
    return { SynchronizationStatus::Error, {} };
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PQNISTL3::Strategy::Encrypt(
    ReadableView buffer, std::uint64_t nonce) const
{
    // Ensure the caller is able to encrypt the buffer with generated session keys. 
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot encrypt before synchronization is complete!");
    }

    // If the buffer contains no data or is less than the specified data to encrypt there is nothing
    // that can be done. 
    if (buffer.size() == 0) { return {}; }

    // Create an OpenSSL encryption context. 
	local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
	if (ERR_get_error() != 0 || !upCipherContext) { return {}; }

    // Get our content encryption key to be used in the cipher. 
    auto const optEncryptionKey = m_store.GetContentKey();
    if (!optEncryptionKey) { return {}; }
    assert(optEncryptionKey->size() == local::EncryptionKeySize);

    // Setup the AES-256-CTR initalization vector from the given nonce. 
	std::array<std::uint8_t, local::EncryptionIVSize> iv = {};
	std::memcpy(iv.data(), &nonce, sizeof(nonce));

    // Initialize the OpenSSL cipher using AES-256-CTR with the encryption key and IV. 
	if (!EVP_EncryptInit_ex(
        upCipherContext.get(), EVP_aes_256_ctr(), nullptr, optEncryptionKey->data(), iv.data())) {
		return {};
	}

    // Sanity check that our encryption key and IV are the size expected by OpenSSL. 
    assert(std::int32_t(optEncryptionKey->size()) == EVP_CIPHER_CTX_key_length(upCipherContext.get()));
    assert(std::int32_t(iv.size()) == EVP_CIPHER_CTX_iv_length(upCipherContext.get()));

	Buffer ciphertext(buffer.size(), 0x00); // Create a buffer to store the encrypted data. 
	auto pCiphertext = ciphertext.data();
	auto const pPlaintext = buffer.data();

    // Encrypt the plaintext into the ciphertext buffer.
    constexpr std::size_t MaximumBlockSize = static_cast<std::size_t>(
        std::numeric_limits<std::int32_t>::max());
    std::size_t encrypted = 0;
    std::int32_t block = static_cast<std::int32_t>(std::min(buffer.size(), MaximumBlockSize));
    for (std::size_t encrypting = buffer.size(); encrypting > 0; encrypting -= block) {
        std::int32_t processed = 0;
        if (!EVP_EncryptUpdate(upCipherContext.get(), pCiphertext, &processed, pPlaintext, block)) {
            return {};
        }
        encrypted += processed;
        block = static_cast<std::int32_t>(std::min(encrypting, MaximumBlockSize));
    }

    // Cleanup the OpenSSL encryption cipher. 
    std::int32_t processed = 0;
	if (!EVP_EncryptFinal_ex(upCipherContext.get(), pCiphertext + encrypted, &processed)) {
		return {};
	}

	return ciphertext;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PQNISTL3::Strategy::Decrypt(
    ReadableView buffer, std::uint64_t nonce) const
{
    // Ensure the caller is able to decrypt the buffer with generated session keys.
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot decrypt before synchronization is complete!");
    }

    // If the buffer contains no data or is less than the specified data to decrypt there is nothing
    // that can be done.
	if (buffer.size() == 0) { return {}; }

    // Create an OpenSSL decryption context.
	local::CipherContext upCipherContext(EVP_CIPHER_CTX_new(), &EVP_CIPHER_CTX_free);
	if (ERR_get_error() != 0 || !upCipherContext) { return {}; }

    // Get the peer's content decryption key to be used in the cipher.
    auto const optDecryptionKey = m_store.GetPeerContentKey();
    if (!optDecryptionKey) { return {}; }
    assert(optDecryptionKey->size() == local::EncryptionKeySize);

    // Setup the AES-256-CTR initalization vector from the given nonce.
	std::array<std::uint8_t, local::EncryptionIVSize> iv = {};
	std::memcpy(iv.data(), &nonce, sizeof(nonce));

    // Initialize the OpenSSL cipher using AES-256-CTR with the decryption key and IV.
	if (!EVP_DecryptInit_ex(
        upCipherContext.get(), EVP_aes_256_ctr(), nullptr, optDecryptionKey->data(), iv.data())) {
		return {};
	}

    // Sanity check that our decryption key and IV are the size expected by OpenSSL.
    assert(std::int32_t(optDecryptionKey->size()) == EVP_CIPHER_CTX_key_length(upCipherContext.get()));
    assert(std::int32_t(iv.size()) == EVP_CIPHER_CTX_iv_length(upCipherContext.get()));

	Buffer plaintext(buffer.size(), 0x00); // Create a buffer to store the decrypted data. 
	auto pPlaintext = plaintext.data();
	auto const pCiphertext = buffer.data();

    // Decrypt the ciphertext into the plaintext buffer.
    constexpr std::size_t MaximumBlockSize = static_cast<std::size_t>(
        std::numeric_limits<std::int32_t>::max());
    std::size_t decrypted = 0;
    std::int32_t block = static_cast<std::int32_t>(std::min(buffer.size(), MaximumBlockSize));
    for (std::size_t encrypting = buffer.size(); encrypting > 0; encrypting -= block) {
        std::int32_t processed = 0;
        if (!EVP_DecryptUpdate(upCipherContext.get(), pPlaintext, &processed, pCiphertext, block)) {
            return {};
        }
        decrypted += processed;
        block = static_cast<std::int32_t>(std::min(encrypting, MaximumBlockSize));
    }

    // Cleanup the OpenSSL encryption cipher. 
    std::int32_t processed = 0;
	if (!EVP_DecryptFinal_ex(upCipherContext.get(), pPlaintext + decrypted, &processed)) {
		return {};
	}

	return plaintext;
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Security::PQNISTL3::Strategy::Sign(Buffer& buffer) const
{
    return Sign(buffer, buffer); // Generate and add the signature to the provided buffer. 
}

//----------------------------------------------------------------------------------------------------------------------

Security::VerificationStatus Security::PQNISTL3::Strategy::Verify(ReadableView buffer) const
{
    // Ensure the caller is able to verify the buffer with generated session keys.
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot decrypt before synchronization is complete!");
    }

    // Determine the amount of non-signature data is packed into the buffer. 
    std::int64_t packContentSize = buffer.size() - SignatureSize;
	if (buffer.empty() || packContentSize <= 0) { return VerificationStatus::Failed; }

    // Get the peer's signature key to be used to generate the expected siganture..
    auto const optPeerSignatureKey = m_store.GetPeerSignatureKey();
    if (!optPeerSignatureKey) { return VerificationStatus::Failed; }
    assert(optPeerSignatureKey->size() == SignatureSize);

    // Create the signature that peer should have provided. 
    ReadableView payload(buffer.begin(), packContentSize);
	auto const optGeneratedBuffer = GenerateSignature(*optPeerSignatureKey, payload);
	if (!optGeneratedBuffer) { return VerificationStatus::Failed; }

    // Compare the generated signature with the signature attached to the buffer. 
	auto const result = CRYPTO_memcmp(
		optGeneratedBuffer->data(), buffer.data() + packContentSize, optGeneratedBuffer->size());

    // If the signatures are not equal than the peer did not sign the buffer or the buffer was
    // altered in transmission. 
	if (result != 0) { return VerificationStatus::Failed; }

	return VerificationStatus::Success;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::PQNISTL3::Strategy::InitializeApplicationContext()
{
    if (!m_spSharedContext) {
        m_spSharedContext = std::make_shared<Context>(KeyEncapsulationScheme);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Security::PQNISTL3::Strategy::ShutdownApplicationContext()
{
    Strategy::m_spSharedContext.reset();
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Security::PQNISTL3::Context> Security::PQNISTL3::Strategy::GetSessionContext() const
{
    return m_spSessionContext;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::PQNISTL3::Strategy::GetPublicKeySize() const
{
    return m_spSessionContext->GetPublicKeySize();
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Generate and encapsulates an ephemeral session key using NTRU-HPS-2048-677. The
// caller is provided the encapsulated shared secret to provide the peer. If an error is 
// encountered nullopt is provided instead. 
//----------------------------------------------------------------------------------------------------------------------
Security::OptionalBuffer Security::PQNISTL3::Strategy::EncapsulateSharedSecret()
{
    // A shared secret cannot be generated and encapsulated without the peer's public key. 
    auto const& optPeerPublicKey = m_store.GetPeerPublicKey();
    if (!optPeerPublicKey) { return {}; }

    // The session context should always exist after the constructor is called. 
    assert(m_spSessionContext);

    Security::Buffer encapsulation;
    // Use the session context to generate a secret for using the peer's public key. 
    bool const success = m_spSessionContext->GenerateEncapsulatedSecret(
        *optPeerPublicKey,
        [this, &encapsulation] (Buffer&& encaped, Buffer&& secret) -> bool
        {
            if (!m_store.GenerateSessionKeys(
                m_role, std::move(secret), local::EncryptionKeySize, SignatureSize)) {
                return false;
            }

            encapsulation = std::move(encaped);
            return true;
        });

    if (!success) { return {}; }

    return encapsulation;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Decapsulates an ephemeral session key using NTRU-HPS-2048-677 from the provided
// encapsulated ciphertext. 
//----------------------------------------------------------------------------------------------------------------------
bool Security::PQNISTL3::Strategy::DecapsulateSharedSecret(Buffer const& encapsulation)
{
    // The session context should always exist after the constructor is called. 
    assert(m_spSessionContext);

    Buffer decapsulation;
    if (!m_spSessionContext->DecapsulateSecret(encapsulation, decapsulation)) { return false; }
    
    return m_store.GenerateSessionKeys(
        m_role, std::move(decapsulation), local::EncryptionKeySize, SignatureSize);
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PQNISTL3::Strategy::GenerateVerficationData()
{
    assert(m_store.HasGeneratedKeys());

    // Get the verification data to provide to encrypt.
    auto const& optData = m_store.GetVerificationData();
    if (!optData) {
        m_synchronization.SetError();
        return {};
    }

    // Encrypt verification data to challenge the peer's keys. 
    Security::OptionalBuffer optEncrypted = Encrypt(*optData, 0);
    if (!optEncrypted) {
        m_synchronization.SetError();
        return {};
    }

    // Provided the encrypted verification data to the caller. 
    return optEncrypted;
}

//----------------------------------------------------------------------------------------------------------------------

Security::VerificationStatus Security::PQNISTL3::Strategy::VerifyKeyShare(Buffer const& buffer) const
{
    // Get our own verification data to verify the provided encrypted data. 
    auto const& optVerificationData = m_store.GetVerificationData();
    if (!optVerificationData) { return VerificationStatus::Failed; }

    // Decrypt the provided data to get the peer's verification data. 
    auto const optDecryptedData = Decrypt(buffer, 0);
    if (!optDecryptedData) { return VerificationStatus::Failed; }

    // Verify the provided verification data matches the verification data we have generated. 
    // If the data does not match, it is an error. 
    bool const bMatchedVerificationData = std::equal(
        optVerificationData->begin(), optVerificationData->end(), optDecryptedData->begin());
    if (!bMatchedVerificationData) { return VerificationStatus::Failed; }

    return VerificationStatus::Success;
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Security::PQNISTL3::Strategy::Sign(
    ReadableView source, Security::Buffer& destination) const
{
    // Ensure the caller is able to sign the buffer with generated session keys.
    if (!m_store.HasGeneratedKeys()) {
        throw std::runtime_error("Security Strategy cannot decrypt before synchronization is complete!");
    }
    
    // Get our signature key to be used when generating the content siganture. .
    auto const optSignatureKey = m_store.GetSignatureKey();
    if (!optSignatureKey) { return -1; }
    assert(optSignatureKey->size() == SignatureSize);

    // Sign the provided buffer with our signature key .
    OptionalBuffer optSignature = GenerateSignature(*optSignatureKey, source);
    if (!optSignature) { return -1; }

    // Insert the signature to create a verifiable buffer. 
    destination.insert(destination.end(), optSignature->begin(), optSignature->end());
    return static_cast<std::int32_t>(optSignature->size());
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PQNISTL3::Strategy::GenerateSignature(
    ReadableView key, ReadableView data) const
{
    // If there is no data to be signed, there is nothing to do. 
	if (data.size() == 0) { return {}; }

    // Hash the provided buffer with the provided key to generate the signature. 
    Buffer signature(SignatureSize, 0x00);
	std::uint32_t hashed = 0;
	HMAC(EVP_sha384(), key.data(), static_cast<std::int32_t>(key.size()),
        data.data(), data.size(), signature.data(), &hashed);

    assert(hashed == static_cast<std::uint32_t>(signature.size()));
	if (ERR_get_error() != 0 || hashed == 0) { return {}; }

    return signature;
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::Strategy::HandleInitiatorSynchronization(
    ReadableView buffer)
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

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::Strategy::HandleInitiatorInitialization(
    ReadableView buffer)
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

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::Strategy::HandleAcceptorSynchronization(
    ReadableView buffer)
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

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::Strategy::HandleAcceptorInitialization(
    ReadableView buffer)
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

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PQNISTL3::Strategy::HandleAcceptorVerification(
    ReadableView buffer)
{
    // Handle the initiator's verification request. As post condition the synchronization will
    // be finalized. If the verfication data could not be verified set an error. 
    if (!Acceptor::HandleVerificationRequest(this, m_synchronization, buffer)) {
        m_synchronization.SetError();
    }

    // There are no further synchronization messages to be provided. 
    return { m_synchronization.GetStatus(), {} };
}

//----------------------------------------------------------------------------------------------------------------------

Security::Strategy local::UnpackStrategy(
    Security::ReadableView::iterator& begin, Security::ReadableView::iterator const& end)
{
    using StrategyType = std::underlying_type_t<Security::Strategy>;

    StrategyType strategy = 0;
    if (!PackUtils::UnpackChunk(begin, end, strategy)) {
        return Security::Strategy::Invalid;
    }
    return Security::ConvertToStrategy(strategy);
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Generate and return a buffer of the provided size filled with random data. 
//----------------------------------------------------------------------------------------------------------------------
Security::OptionalBuffer local::GenerateRandomData(std::size_t size)
{
    assert(std::in_range<std::int32_t>(size));
    Security::Buffer buffer = std::vector<std::uint8_t>(size, 0x00);
    if (!RAND_bytes(buffer.data(), static_cast<std::int32_t>(size))) { return {}; }
    return buffer;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Initiator::GenerateInitializationRequest(
    Security::PQNISTL3::Context const& context,
    Security::KeyStore& store,
    Security::PQNISTL3::SynchronizationTracker& synchronization)
{
    using namespace Security::PQNISTL3;

    // Generate our prinicpal seed for the session. 
    auto const optPrincipalSeed = local::GenerateRandomData(Strategy::PrincipalRandomSize);
    if (!optPrincipalSeed) { return {}; }

    // Expand the KeyStore's deriviation seed. 
    store.ExpandSessionSeed(*optPrincipalSeed);

    constexpr std::size_t RequestSize = 
        sizeof(Strategy::Type) + Strategy::PrincipalRandomSize + Context::PublicKeySize;

    Security::Buffer request;
    request.reserve(RequestSize);

    PackUtils::PackChunk(Strategy::Type, request); // Pack the strategy type.
    PackUtils::PackChunk(*optPrincipalSeed, request);
    if (std::size_t fetched = context.GetPublicKey(request); fetched == 0) { return {}; }

    // Update the synchronization's plaintext verification buffer with the request
    synchronization.UpdateTransaction(request);

    return request;
}

//----------------------------------------------------------------------------------------------------------------------

bool Initiator::HandleInitializationResponse(
    Security::PQNISTL3::Strategy* const pStrategy,
    Security::KeyStore& store,
    Security::PQNISTL3::SynchronizationTracker& synchronization,
    Security::ReadableView request)
{
    using namespace Security::PQNISTL3;

    assert(pStrategy);

	auto begin = request.begin();
	auto const end = request.end();

    Security::Strategy strategy = local::UnpackStrategy(begin, end);
    if (strategy != Strategy::Type) { return {}; }

    constexpr std::size_t ExpectedRequestSize = 
        sizeof(Strategy::Type) + Strategy::PrincipalRandomSize + Context::EncapsulationSize +
        Security::KeyStore::VerificationSize + Strategy::SignatureSize;
    if (request.size() != ExpectedRequestSize) { return false; }

    // Handle the peer's packed prinicpal random seed. 
    {
        Security::Buffer seed;
        seed.reserve(Strategy::PrincipalRandomSize);
        if (!PackUtils::UnpackChunk(begin, end, seed)) { return false; }

        // Expand the key store's deriviation seed with the provided data. 
        store.ExpandSessionSeed(seed);
    }

    // Handle the peer's packed encapsulation data. 
    {
        Security::Buffer encapsulation;
        encapsulation.reserve(Context::EncapsulationSize);
        if (!PackUtils::UnpackChunk(begin, end, encapsulation)) { return false; }

        // Attempt to decapsulate the shared secret. If the shared secret could not be decapsulated
        // or the session keys fail to be generated return an error. 
        if (!pStrategy->DecapsulateSharedSecret(encapsulation)) { return false; }
    }

    // Handle the peer's verification data.
    {
        Security::Buffer verification;
        verification.reserve(Security::KeyStore::VerificationSize);
        if (!PackUtils::UnpackChunk(begin, end, verification)) { return false; }

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

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Initiator::GeneratVerificationRequest(
    Security::PQNISTL3::Strategy* const pStrategy,
    Security::PQNISTL3::SynchronizationTracker& synchronization)
{
    using namespace Security::PQNISTL3;

    assert(pStrategy);

    auto const& optVerification = pStrategy->GenerateVerficationData();
    if (!optVerification) { return {}; }

    constexpr std::size_t RequestSize  = 
        sizeof(Strategy::Type) + Security::KeyStore::VerificationSize +
        Strategy::SignatureSize;

    Security::Buffer request;
    request.reserve(RequestSize);

    PackUtils::PackChunk(Strategy::Type, request);
    PackUtils::PackChunk(*optVerification, request);

    // If for some reason we cannot sign the verification data it is an error. 
    if (pStrategy->Sign(request) == 0) { return {}; }

    // The synchronization process is now complete.
    synchronization.Finalize(Strategy::InitiatorStage::Complete);

    return request;
}

//----------------------------------------------------------------------------------------------------------------------

bool Acceptor::HandleInitializationRequest(
    Security::PQNISTL3::Context const& context,
    Security::KeyStore& store,
    Security::PQNISTL3::SynchronizationTracker& synchronization,
    Security::ReadableView request)
{
    using namespace Security::PQNISTL3;

	auto begin = request.begin();
	auto const end = request.end();

    Security::Strategy strategy = local::UnpackStrategy(begin, end);
    if (strategy != Strategy::Type) { return false; }

    constexpr std::size_t ExpectedRequestSize = 
        sizeof(Strategy::Type) + Strategy::PrincipalRandomSize + Context::PublicKeySize;
    if (request.size() != ExpectedRequestSize) { return false; }

    // Handle the peer's packed prinicpal random seed. 
    {
        Security::Buffer seed;
        seed.reserve(Strategy::PrincipalRandomSize);
        if (!PackUtils::UnpackChunk(begin, end, seed)) { return false; }

        // Expand the key store's deriviation seed with the provided data. 
        store.ExpandSessionSeed(seed);
    }

    // Handle the peer's packed publick key.
    {
        Security::Buffer key;
        auto const size = context.GetPublicKeySize();
        key.reserve(size);
        if (!PackUtils::UnpackChunk(begin, end, key)) { return false; }
        
        store.SetPeerPublicKey(std::move(key)); // Store the peer's public key.
    }

    // Add the request to the synchronization transaction. 
    synchronization.UpdateTransaction(request);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Acceptor::GenerateInitializationResponse(
    Security::PQNISTL3::Strategy* const pStrategy,
    Security::KeyStore& store,
    Security::PQNISTL3::SynchronizationTracker& synchronization)
{
    using namespace Security::PQNISTL3;

    assert(pStrategy);

    // Generate random data to be used to generate the session keys. 
    auto const optPrincipalSeed = local::GenerateRandomData(Strategy::PrincipalRandomSize);
    if (!optPrincipalSeed) { return {}; }

    // Add the prinicpal random data to the store in order to use it when generating session keys. 
    store.ExpandSessionSeed(*optPrincipalSeed);

    // Create an encapsulated shared secret using the peer's public key. If the process fails,
    // the synchronization failed and we cannot proceed. 
    auto optEncapsulation = pStrategy->EncapsulateSharedSecret();
    if (!optEncapsulation) { return {}; }

    // Generate the verififcation data needed for the response. If the process fails, return an error. 
    auto optVerification = pStrategy->GenerateVerficationData();
    if (!optVerification) { return {}; }

    constexpr std::size_t ResponseSize  = 
        sizeof(Strategy::Type) + Strategy::PrincipalRandomSize + Context::EncapsulationSize +
        Security::KeyStore::VerificationSize + Strategy::SignatureSize;

    Security::Buffer response; 
    response.reserve(ResponseSize);

    PackUtils::PackChunk(Strategy::Type, response);
    PackUtils::PackChunk(*optPrincipalSeed, response);
	PackUtils::PackChunk(*optEncapsulation, response);
	PackUtils::PackChunk(*optVerification, response);

    synchronization.SetStage(Strategy::AcceptorStage::Verification);
    if (!synchronization.SignTransaction(response)) { return {}; }

    return response;
}

//----------------------------------------------------------------------------------------------------------------------

bool Acceptor::HandleVerificationRequest(
    Security::PQNISTL3::Strategy* const pStrategy,
    Security::PQNISTL3::SynchronizationTracker& synchronization,
    Security::ReadableView request)
{
    using namespace Security::PQNISTL3;

    assert(pStrategy);

    // In the acceptor's verification stage we expect to have been provided the initator's 
    // signed verfiection data. If the buffer could not be verified, it is an error. 
    if (pStrategy->Verify(request) != Security::VerificationStatus::Success) {
        return false;
    }

	auto begin = request.begin();
	auto const end = request.end();

    Security::Strategy strategy = local::UnpackStrategy(begin, end);
    if (strategy != Strategy::Type) { return {}; }

    constexpr std::size_t ExpectedRequestSize  = 
        sizeof(Strategy::Type) + Security::KeyStore::VerificationSize +
        Strategy::SignatureSize;
    if (request.size() != ExpectedRequestSize) { return false; }

    // Handle the peer's verification data.
    {
        Security::Buffer verification;
        verification.reserve(Security::KeyStore::VerificationSize);
        if (!PackUtils::UnpackChunk(begin, end, verification)) { return false; }

        // Verify the packed and encrypted verification data. 
        if (pStrategy->VerifyKeyShare(verification) != Security::VerificationStatus::Success) { 
            return false; 
        }
    }

    // The synchronization process is now complete.
    synchronization.Finalize(Strategy::AcceptorStage::Complete);

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
