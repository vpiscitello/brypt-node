//----------------------------------------------------------------------------------------------------------------------
// File: PackageSynchronizer.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "PackageSynchronizer.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "CipherService.hpp"
#include "SynchronizerModel.hpp"
#include "Classical/FiniteFieldDiffieHellmanModel.hpp"
#include "Classical/EllipticCurveDiffieHellmanModel.hpp"
#include "PostQuantum/KeyEncapsulationModel.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Message/PackUtils.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <oqs/kem.h>
#include <openssl/ecdh.h>
#include <openssl/rsa.h>
#include <openssl/dh.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
//----------------------------------------------------------------------------------------------------------------------
#include <array>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

using namespace Security;
using SuiteComponent = std::pair<ConfidentialityLevel, std::string>;
using ComponentPredicate = std::function<
    std::optional<std::string>(std::vector<std::string> const& values, Configuration::Options::Algorithms const& algorithms)>;

template <typename StageEnum>
class Tracker;

enum class KeyAgreementModel : std::int32_t { Classical, PostQuantum };

std::unique_ptr<Detail::ISynchronizerModel> CreateSynchronizerModel(std::string const& keyAgreement);

SuiteComponent ParseAndMatchSuiteComponent(
    ReadableView::const_iterator& begin,
    ReadableView::const_iterator const& end,
    Configuration::Options::SupportedAlgorithms const& supportedAlgorithms,
    ComponentPredicate const& predicate);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

template <typename StageEnum>
class local::Tracker
{
public:
    static_assert(std::is_enum_v<std::decay_t<StageEnum>>, "The synchronization tracker should only instantiated with enum types!");

    using TransactionSignatory = std::function<bool(ReadableView, Buffer&)>;
    using TransactionVerifier = std::function<VerificationStatus(ReadableView buffer)>;

    Tracker();

    SynchronizationStatus GetStatus() const;
    void SetError();

    [[nodiscard]] StageEnum GetStage() const;

    template<typename... Buffers>
    void SetStage(StageEnum stage, Buffers... buffers);

    void SetSignatory(TransactionSignatory const& signatory);
    void SetVerifier(TransactionVerifier const& verifier);
    [[nodiscard]] bool SignTransaction(Buffer& message);
    [[nodiscard]] VerificationStatus VerifyTransaction(ReadableView message);

    void Finalize(StageEnum stage);

    [[nodiscard]] bool ResetState();

private:
    SynchronizationStatus m_status;
    StageEnum m_stage;
    
    SecureBuffer m_transaction;
    TransactionSignatory m_signatory;
    TransactionVerifier m_verifier;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::PackageSynchronizer::InitiatingRoleExecutor : public Security::PackageSynchronizer::ISynchronizationRoleExecutor
{
public:
    enum class Stage : std::uint32_t { CipherSuiteSelection, KeyVerification, Synchronized };
    static constexpr std::uint32_t StageCount = 2; 

    explicit InitiatingRoleExecutor(std::reference_wrapper<Detail::SynchronizerContext>&& context);

    [[nodiscard]] virtual std::uint32_t GetStages() const override;
    [[nodiscard]] virtual SynchronizationStatus GetStatus() const override;
    [[nodiscard]] virtual bool Synchronized() const override;

    [[nodiscard]] virtual SynchronizationResult Initialize() override;
    [[nodiscard]] virtual SynchronizationResult Synchronize(ReadableView buffer) override;

    virtual void SetContext(std::reference_wrapper<Detail::SynchronizerContext>&& context) override
    {
        m_context = std::move(context);
    }

private:
    [[nodiscard]] OptionalBuffer GenerateCipherSuiteSelectionRequest();

    [[nodiscard]] SynchronizationResult ExecuteCipherSuiteSelectionStage(ReadableView response);
    [[nodiscard]] OptionalBuffer OnCipherSuiteSelectionResponse(ReadableView response);

    [[nodiscard]] SynchronizationResult ExecuteVerificationStage(ReadableView response);
    [[nodiscard]] bool OnVerificationResponse(ReadableView response);

    std::reference_wrapper<Detail::SynchronizerContext> m_context;
    local::Tracker<Stage> m_tracker;
    std::unique_ptr<Detail::ISynchronizerModel> m_upModel;
};

//----------------------------------------------------------------------------------------------------------------------

class Security::PackageSynchronizer::AcceptingRoleExecutor : public Security::PackageSynchronizer::ISynchronizationRoleExecutor
{
public:
    enum class Stage : std::uint32_t { CipherSuiteSelection, KeyExchange, Synchronized };
    static constexpr std::uint32_t StageCount = 3; 

    explicit AcceptingRoleExecutor(std::reference_wrapper<Detail::SynchronizerContext>&& context);

    [[nodiscard]] virtual std::uint32_t GetStages() const override;
    [[nodiscard]] virtual SynchronizationStatus GetStatus() const override;
    [[nodiscard]] virtual bool Synchronized() const override;

    [[nodiscard]] virtual SynchronizationResult Initialize() override;
    [[nodiscard]] virtual SynchronizationResult Synchronize(ReadableView buffer) override;

    virtual void SetContext(std::reference_wrapper<Detail::SynchronizerContext>&& context) override
    {
        m_context = std::move(context);
    }

private:
    [[nodiscard]] SynchronizationResult ExecuteCipherSuiteSelectionStage(ReadableView request);
    [[nodiscard]] OptionalBuffer OnCipherSuiteSelectionRequest(ReadableView request);

    [[nodiscard]] SynchronizationResult ExecuteKeyExchangeStage(ReadableView request);
    [[nodiscard]] OptionalBuffer OnKeyExchangeRequest(ReadableView request);

    std::reference_wrapper<Detail::SynchronizerContext> m_context;
    local::Tracker<Stage> m_tracker;
    std::unique_ptr<Detail::ISynchronizerModel> m_upModel;
};

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Security::PackageSynchronizer {
//----------------------------------------------------------------------------------------------------------------------

Security::PackageSynchronizer::PackageSynchronizer(ExchangeRole role, std::weak_ptr<Configuration::Options::SupportedAlgorithms> const& wpSupportedAlgorithms)
    : m_context(role, wpSupportedAlgorithms)
    , m_upExecutor()
{
    switch (role) {
        case Security::ExchangeRole::Initiator: {
            m_upExecutor = std::make_unique<InitiatingRoleExecutor>(std::ref(m_context));
        } break;
        case Security::ExchangeRole::Acceptor: {
            m_upExecutor = std::make_unique<AcceptingRoleExecutor>(std::ref(m_context));
        } break;
        default: assert(false); break;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Security::PackageSynchronizer::PackageSynchronizer(PackageSynchronizer&& other) noexcept
    : m_context(std::move(other.m_context))
    , m_upExecutor(std::move(other.m_upExecutor))
{
    m_upExecutor->SetContext(std::ref(m_context));
}

//----------------------------------------------------------------------------------------------------------------------
Security::PackageSynchronizer& Security::PackageSynchronizer::operator=(PackageSynchronizer&& other) noexcept
{
    m_context = std::move(other.m_context);
    m_upExecutor = std::move(other.m_upExecutor);
    m_upExecutor->SetContext(std::ref(m_context));
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Security::ExchangeRole Security::PackageSynchronizer::GetExchangeRole() const { return m_context.GetExchangeRole(); }

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Security::PackageSynchronizer::GetStages() const { return m_upExecutor->GetStages(); }

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationStatus Security::PackageSynchronizer::GetStatus() const { return m_upExecutor->GetStatus(); }

//----------------------------------------------------------------------------------------------------------------------

bool Security::PackageSynchronizer::Synchronized() const { return m_upExecutor->Synchronized(); }

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::Initialize() { return m_upExecutor->Initialize(); }

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::Synchronize(Security::ReadableView buffer)
{
    return m_upExecutor->Synchronize(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Security::CipherPackage> Security::PackageSynchronizer::Finalize()
{
    return (m_upExecutor->Synchronized()) ? m_context.ReleaseCipherPackage() : nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

void Security::PackageSynchronizer::PackAndCacheSupportedAlgorithms(
    Configuration::Options::SupportedAlgorithms const& supportedAlgorithms)
{
    CachedSupportedAlgorithmsBuffer.clear();

    std::vector<std::string> keyAgreements;
    std::size_t bytesUsedByKeyAgreements = 0;

    std::vector<std::string> ciphers;
    std::size_t bytesUsedByCiphers = 0;

    std::vector<std::string> hashFunctions;
    std::size_t bytesUsedByHashFunctions = 0;

    supportedAlgorithms.ForEachSupportedAlgorithm([&] (ConfidentialityLevel level, Configuration::Options::Algorithms const& algorithms) {
        {
            auto const& supported = algorithms.GetKeyAgreements();
            keyAgreements.reserve(keyAgreements.size() + supported.size());
            keyAgreements.insert(keyAgreements.end(), supported.begin(), supported.end());
            bytesUsedByKeyAgreements += std::accumulate(
                supported.begin(), supported.end(), std::size_t{ 0 },
                [] (std::size_t size, std::string const& value) {
                    return size + value.size() + sizeof(std::uint16_t); // Return the preceding size plus the element's size, add two bytes for the size prefix. 
                });
        }
        {
            auto const& supported = algorithms.GetCiphers();
            ciphers.reserve(ciphers.size() + supported.size());
            ciphers.insert(ciphers.end(), supported.begin(), supported.end());
            bytesUsedByCiphers += std::accumulate(
                supported.begin(), supported.end(), std::size_t{ 0 },
                [] (std::size_t size, std::string const& value) {
                    return size + value.size() + sizeof(std::uint16_t); // Return the preceding size plus the element's size, add two bytes for the size prefix. 
                });
        }
        {
            auto const& supported = algorithms.GetHashFunctions();
            hashFunctions.reserve(hashFunctions.size() + supported.size());
            hashFunctions.insert(hashFunctions.end(), supported.begin(), supported.end());
            bytesUsedByHashFunctions += std::accumulate(
                supported.begin(), supported.end(), std::size_t{ 0 },
                [] (std::size_t size, std::string const& value) {
                    return size + value.size() + sizeof(std::uint16_t); // Return the preceding size plus the element's size, add two bytes for the size prefix. 
                });
        }

        return CallbackIteration::Continue;
    });

    assert(std::in_range<std::uint16_t>(keyAgreements.size()));
    assert(std::in_range<std::uint16_t>(bytesUsedByKeyAgreements));
    PackUtils::PackChunk(static_cast<std::uint16_t>(keyAgreements.size()), CachedSupportedAlgorithmsBuffer);
    PackUtils::PackChunk(static_cast<std::uint16_t>(bytesUsedByKeyAgreements), CachedSupportedAlgorithmsBuffer);
    for (auto const& keyAgreement : keyAgreements) {
        assert(std::in_range<std::uint16_t>(keyAgreement.size()));
        PackUtils::PackChunk<std::uint16_t>(keyAgreement, CachedSupportedAlgorithmsBuffer);
    }

    assert(std::in_range<std::uint16_t>(ciphers.size()));
    assert(std::in_range<std::uint16_t>(bytesUsedByCiphers));
    PackUtils::PackChunk(static_cast<std::uint16_t>(ciphers.size()), CachedSupportedAlgorithmsBuffer);
    PackUtils::PackChunk(static_cast<std::uint16_t>(bytesUsedByCiphers), CachedSupportedAlgorithmsBuffer);
    for (auto const& cipher : ciphers) {
        assert(std::in_range<std::uint16_t>(cipher.size()));
        PackUtils::PackChunk<std::uint16_t>(cipher, CachedSupportedAlgorithmsBuffer);
    }

    assert(std::in_range<std::uint16_t>(hashFunctions.size()));
    assert(std::in_range<std::uint16_t>(bytesUsedByHashFunctions));
    PackUtils::PackChunk(static_cast<std::uint16_t>(hashFunctions.size()), CachedSupportedAlgorithmsBuffer);
    PackUtils::PackChunk(static_cast<std::uint16_t>(bytesUsedByHashFunctions), CachedSupportedAlgorithmsBuffer);
    for (auto const& hashFunction : hashFunctions) {
        assert(std::in_range<std::uint16_t>(hashFunction.size()));
        PackUtils::PackChunk<std::uint16_t>(hashFunction, CachedSupportedAlgorithmsBuffer);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// } Security::PackageSynchronizer
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// local::Tracker {
//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
local::Tracker<StageEnum>::Tracker()
    : m_status(SynchronizationStatus::Processing)
    , m_stage(static_cast<StageEnum>(0))
    , m_transaction()
    , m_signatory()
    , m_verifier()
{
}

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
Security::SynchronizationStatus local::Tracker<StageEnum>::GetStatus() const { return m_status; }

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
void local::Tracker<StageEnum>::SetError() { m_status = SynchronizationStatus::Error; }

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
StageEnum local::Tracker<StageEnum>::GetStage() const { return m_stage; }

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
template<typename... Buffers>
void local::Tracker<StageEnum>::SetStage(StageEnum stage, Buffers... buffers)
{
    m_stage = stage;
    m_transaction.Append(buffers...);
}

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
void local::Tracker<StageEnum>::SetSignatory(TransactionSignatory const& signatory) { m_signatory = signatory; }

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
void local::Tracker<StageEnum>::SetVerifier(TransactionVerifier const& verifier) { m_verifier = verifier; }

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
bool local::Tracker<StageEnum>::SignTransaction(Buffer& message)
{
    assert(m_signatory); // The strategy should always have provided us a signatory. 
    m_transaction.Append(message);

    std::size_t const size = message.size();
    bool const success = m_signatory(m_transaction.GetData(), message);
    m_transaction.Append(ReadableView{ message.begin() + size, message.end() });  // Append the signature to the transaction.

    return success;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
Security::VerificationStatus local::Tracker<StageEnum>::VerifyTransaction(ReadableView message)
{
    assert(m_verifier); // The strategy should always have provided us a verifier. 
    m_transaction.Append(message);
    return m_verifier(m_transaction.GetData());
}

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
void local::Tracker<StageEnum>::Finalize(StageEnum stage)
{
    m_status = SynchronizationStatus::Ready;
    m_transaction.Erase();
    SetStage(stage);
}

//----------------------------------------------------------------------------------------------------------------------

template<typename StageEnum>
bool local::Tracker<StageEnum>::ResetState()
{
    m_status = SynchronizationStatus::Processing;
    m_stage = static_cast<StageEnum>(0);
    m_transaction.Erase();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// } local::Tracker
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Security::PackageSynchronizer::InitiatingRoleExecutor {
//----------------------------------------------------------------------------------------------------------------------

Security::PackageSynchronizer::InitiatingRoleExecutor::InitiatingRoleExecutor(std::reference_wrapper<Detail::SynchronizerContext>&& context)
    : m_context(std::move(context))
    , m_tracker()
    , m_upModel()
{
    m_tracker.SetSignatory([this] (ReadableView transaction, Buffer& message) -> bool { 
        auto const& upCipherPackage = m_context.get().GetCipherPackage();
        if (!upCipherPackage) {
            throw std::runtime_error("Attempted to sign the key exchange result before the cipher package was created!");
        }
        return upCipherPackage->Sign(transaction, message);
    });

    m_tracker.SetVerifier([this] (ReadableView transaction) -> VerificationStatus { 
        auto const& upCipherPackage = m_context.get().GetCipherPackage();
        if (!upCipherPackage) {
            throw std::runtime_error("Attempted to verify the key exchange result before the cipher package was created!");
        }
        return upCipherPackage->Verify(transaction);
    });
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Security::PackageSynchronizer::InitiatingRoleExecutor::GetStages() const { return StageCount; }

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationStatus Security::PackageSynchronizer::InitiatingRoleExecutor::GetStatus() const { return m_tracker.GetStatus(); }

//----------------------------------------------------------------------------------------------------------------------

bool Security::PackageSynchronizer::InitiatingRoleExecutor::Synchronized() const { return m_tracker.GetStatus() == SynchronizationStatus::Ready; }

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::InitiatingRoleExecutor::Initialize()
{
    // Generate the synchronization request with our context and seed. 
    OptionalBuffer optRequest = GenerateCipherSuiteSelectionRequest();
    if (!optRequest) {
        m_tracker.SetError();
        return { m_tracker.GetStatus(), {} };
    }

    m_tracker.SetStage(Stage::CipherSuiteSelection, *optRequest);

    return { m_tracker.GetStatus(), *optRequest };
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::InitiatingRoleExecutor::Synchronize(Security::ReadableView response)
{
    switch (m_tracker.GetStage()) {
        case Stage::CipherSuiteSelection: { return ExecuteCipherSuiteSelectionStage(response); }
        case Stage::KeyVerification: { return ExecuteVerificationStage(response); }
        // It is an error to be called in all other synchronization stages. 
        case Stage::Synchronized: {
            m_tracker.SetError();
            return { m_tracker.GetStatus(), {} };
        }
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PackageSynchronizer::InitiatingRoleExecutor::GenerateCipherSuiteSelectionRequest()
{
    if (CachedSupportedAlgorithmsBuffer.empty()) {
        return {}; // If the cached buffer is empty, an error has occurred. 
    }

    return CachedSupportedAlgorithmsBuffer;
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::InitiatingRoleExecutor::ExecuteCipherSuiteSelectionStage(
    ReadableView response)
{
    auto optRequest = OnCipherSuiteSelectionResponse(response);

    if (!optRequest) {
        m_tracker.SetError();
        return { m_tracker.GetStatus(), {} };
    }

    m_tracker.SetStage(Stage::KeyVerification, response);

    // Sign the entire transaction and inject the signature into the request such that the acceptor can verify. 
    if (!m_tracker.SignTransaction(*optRequest)) {
        m_tracker.SetError();
        return {};
    }

    return { m_tracker.GetStatus(), std::move(*optRequest) };
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PackageSynchronizer::InitiatingRoleExecutor::OnCipherSuiteSelectionResponse(ReadableView response)
{
    constexpr std::size_t minimumRequestSize = [] () -> std::size_t {
        std::size_t size = 0;
        size += sizeof(std::uint16_t); // 2 bytes for the size of the key agreement name
        size += sizeof(std::uint16_t); // 2 bytes for the size of the cipher name
        size += sizeof(std::uint16_t); // 2 bytes for the size of the hash function name
        return size;
    }();

    constexpr std::size_t maximumRequestSize = [] () -> std::size_t {
        std::size_t size = 0;
        size += sizeof(std::uint16_t); // 2 bytes for the size of the key agreement name
        size += MaximumSupportedAlgorithmNameSize; // N bytes for the maximum size of the key agreement name
        size += sizeof(std::uint16_t); // 2 bytes for the size of the cipher name
        size += MaximumSupportedAlgorithmNameSize; // N bytes for the maximum size of the cipher name
        size += sizeof(std::uint16_t); // 2 bytes for the size of the hash function name
        size += MaximumSupportedAlgorithmNameSize; // N bytes for the maximum size of the hash function name
        size += sizeof(std::uint32_t); // 4 bytes for the size of the hash function name
        size += MaximumExpectedPublicKeySize; // N bytes for the maximum size of the hash function name
        size += sizeof(std::uint16_t); // 2 bytes for the size of the hash function name
        size += MaximumExpectedSaltSize; // N bytes for the maximum size of the hash function name
        return size;
    }();

    if (response.size() < minimumRequestSize || response.size() > maximumRequestSize) {
        return {}; // If the response size is not within the expected bounds, an error has occurred. 
    }

    auto begin = response.begin();
	auto const end = response.end();

    std::string const keyAgreement = [&begin, &end] () -> std::string {
		std::uint16_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return ""; }

        std::string value;
		if (!PackUtils::UnpackChunk(begin, end, value, size)) { return ""; }

        return value;
    }();

    if (keyAgreement.empty()) {
        return {}; // If the key agreement name is empty after parsing, an error has occurred. 
    }

    std::string const cipher = [&begin, &end] () -> std::string {
		std::uint16_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return ""; }

        std::string value;
		if (!PackUtils::UnpackChunk(begin, end, value, size)) { return ""; }

        return value;
    }();

    if (cipher.empty()) {
        return {}; // If the key agreement name is empty after parsing, an error has occurred. 
    }

    std::string const hashFunction = [&begin, &end] () -> std::string {
		std::uint16_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return ""; }

        std::string value;
		if (!PackUtils::UnpackChunk(begin, end, value, size)) { return ""; }

        return value;
    }();

    if (hashFunction.empty()) {
        return {}; // If the key agreement name is empty after parsing, an error has occurred. 
    }

    // Provide the received algorithm names to the cipher service such that a mutual cipher suite may be created. 
    auto optCipherSuite = m_context.get().CreateMutualCipherSuite(keyAgreement, cipher, hashFunction);
    if (!optCipherSuite) {
        return {}; // If a mutual cipher suite could not be created, an error has occurred. 
    }
    
    if (m_upModel = local::CreateSynchronizerModel(keyAgreement); !m_upModel) {
        return {}; // If we have failed to make a synchronizer model, an error has occurred. 
    }

    auto optPublicKey = m_upModel->SetupKeyExchange(*optCipherSuite);
    if (!optPublicKey) {
        return {}; // If the model indicates it could not setup the key exchange, an error has occurred. 
    }

    std::size_t expectedPublicKeyAndSaltSize = [&] () -> std::size_t {
        std::size_t size = 0;
        size += sizeof(std::uint32_t); // 4 bytes for the public key size.
        size += optPublicKey->GetSize(); // N bytes for the public key.
        size += sizeof(std::uint16_t); // 2 bytes for the salt size. 
        size += m_context.get().GetSaltSize(); // N bytes for the salt. 
        return size;
    }();

    if (std::cmp_not_equal(std::distance(begin, end), expectedPublicKeyAndSaltSize)) {
        return {}; // If the data to be read does not equal the expect size of the public key and salt, an error has ocurred. 
    }

    // Now that we've determined that a valid key exchange model has been made and we have the data necessary, we can 
    // start to build up the request that will be sent back to the peer. 
    std::size_t const requestSize = [&] () -> std::size_t {
        std::size_t size = 0;
        size += sizeof(std::uint16_t); // 1 byte for the key agreement name size. 
        size += keyAgreement.size(); // N bytes for the key agreement name. 
        size += sizeof(std::uint16_t); // 1 byte for the cipher name size. 
        size += cipher.size(); //  N bytes for the cipher name. 
        size += sizeof(std::uint16_t); // 1 byte for the hash function name size. 
        size += hashFunction.size(); // N bytes for the hash function name. 
        size += sizeof(std::uint32_t); // 4 bytes for the public key size.
        size += optPublicKey->GetSize(); // N bytes for the public key.
        size += sizeof(std::uint16_t); // 2 bytes for the salt size. 
        size += m_context.get().GetSaltSize(); // N bytes for the salt. 
        size += m_upModel->GetSupplementalDataSize(); // N bytes for the supplemental data used by the model. 
        size += m_context.get().GetVerificationDataSize(); // N bytes for the verification data. 
        return size;
    }();

    Buffer request;
    PackUtils::PackChunk<std::uint16_t>(keyAgreement, request); // Pack the name of the key agreement.
    PackUtils::PackChunk<std::uint16_t>(cipher, request); // Pack the name of the cipher.
    PackUtils::PackChunk<std::uint16_t>(hashFunction, request); // Pack the name of the hash function.
    PackUtils::PackChunk<std::uint32_t>(optPublicKey->GetData(), request);
    
    // Setup the key store for the synchronization process. 
    {
        auto const& salt = m_context.get().SetupKeyShare(std::move(optCipherSuite), std::move(*optPublicKey)); 
        PackUtils::PackChunk<std::uint16_t>(salt.GetData(), request);
    }

    auto peerPublicKey = [&begin, &end, &context = m_context] () -> PublicKey {
        std::uint32_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return {}; }

        std::size_t const expected = context.get().GetPublicKeySize();
        if (size != expected) { return {}; }

        Buffer buffer;
        if (!PackUtils::UnpackChunk(begin, end, buffer, size)) { return {}; }

        return PublicKey{ std::move(buffer) };
    }();

    if (peerPublicKey.IsEmpty()) {
        return {}; // If we failed to parse a public key from the buffer, an error has occurred. 
    }

    auto peerSalt = [&begin, &end, &context = m_context] () -> Salt {
        std::uint16_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return {}; }

        std::size_t const expected = context.get().GetSaltSize();
        if (size != expected) { return {}; }

        Buffer buffer;
        if (!PackUtils::UnpackChunk(begin, end, buffer, size)) { return {}; }

        return Salt{ std::move(buffer) };
    }();

    if (peerSalt.IsEmpty()) {
        return {}; // If we failed to parse salt from the buffer, an error has occurred. 
    }

    auto optResult = m_upModel->ComputeSharedSecret(peerPublicKey);
    if (!optResult) {
        return {}; // If the model failed to compute a shared secret using the peer's public key, an error has occurred. 
    }

    // We are done using the peer's public key and salt, we may now move it into the context. 
    if (!m_context.get().SetPeerPublicKeyAndSalt(std::move(peerPublicKey), std::move(peerSalt))) {
        return {}; // If we fail to store the peer's public key and salt an error has occurred. 
    }

    auto&& [sharedSecret, supplementalData] = *optResult;

    // Note: After this point it is no longer valid to use the key store as it has been moved into the 
    // generated cipher package. 
    auto const optVerificationData = m_context.get().GenerateSessionKeys(std::move(sharedSecret));
    if (!optVerificationData) {
        return {}; // If the we failed to generate the session keys, an error has occureed. 
    }

    PackUtils::PackChunk(supplementalData.GetData(), request);
    PackUtils::PackChunk(*optVerificationData, request);

    return request;
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::InitiatingRoleExecutor::ExecuteVerificationStage(
    ReadableView response)
{
    // Handle the acceptor's response to the initialization message. The post conditions for 
    // this handling include generating the session keys and verifying the plaintext transaction. 
    bool const success = OnVerificationResponse(response);

    // Set an error if the response could not be handled. 
    if (!success) {
        m_tracker.SetError();
        return { m_tracker.GetStatus(), {} };
    }

    if (m_tracker.VerifyTransaction(response) != VerificationStatus::Success) {
        m_tracker.SetError();
        return { m_tracker.GetStatus(), {} };
    }

    m_tracker.Finalize(Stage::Synchronized); // The synchronization process is now complete.

    return { m_tracker.GetStatus(), {} };
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::PackageSynchronizer::InitiatingRoleExecutor::OnVerificationResponse(ReadableView response)
{
    std::size_t const expected = [&] () -> std::size_t {
        std::size_t size = 0;
        size += m_context.get().GetVerificationDataSize(); // N bytes for the verification data. 
        size += m_context.get().GetSignatureSize(); // N bytes for the signature. 
        return size;
    }();

    if (response.size() != expected) { return false; }

    auto const& upCipherPackage = m_context.get().GetCipherPackage();
    if (!upCipherPackage) {
        return {}; // If we do not have a cipher packacge, an error occurred. 
    }

	auto begin = response.begin();
	auto const end = response.end();

    // Verify the packed and encrypted verification data. 
    if (m_context.get().VerifyKeyShare(ReadableView{ begin, m_context.get().GetVerificationDataSize() }) != VerificationStatus::Success) {
        return false;
    }
 
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// } Security::PackageSynchronizer::InitiatingRoleExecutor
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Security::PackageSynchronizer::AcceptingRoleExecutor {
//----------------------------------------------------------------------------------------------------------------------

Security::PackageSynchronizer::AcceptingRoleExecutor::AcceptingRoleExecutor(std::reference_wrapper<Detail::SynchronizerContext>&& context)
    : m_context(std::move(context))
    , m_tracker()
    , m_upModel()
{
    m_tracker.SetSignatory([this] (ReadableView transaction, Buffer& message) -> bool { 
        auto const& upCipherPackage = m_context.get().GetCipherPackage();
        if (!upCipherPackage) {
            throw std::runtime_error("Attempted to sign the key exchange result before the cipher package was created!");
        }
        return upCipherPackage->Sign(transaction, message);
    });

    m_tracker.SetVerifier([this] (ReadableView transaction) -> VerificationStatus { 
        auto const& upCipherPackage = m_context.get().GetCipherPackage();
        if (!upCipherPackage) {
            throw std::runtime_error("Attempted to verify the key exchange result before the cipher package was created!");
        }
        return upCipherPackage->Verify(transaction);
    });
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Security::PackageSynchronizer::AcceptingRoleExecutor::GetStages() const { return StageCount; }

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationStatus Security::PackageSynchronizer::AcceptingRoleExecutor::GetStatus() const { return m_tracker.GetStatus(); }

//----------------------------------------------------------------------------------------------------------------------

bool Security::PackageSynchronizer::AcceptingRoleExecutor::Synchronized() const { return m_tracker.GetStatus() == SynchronizationStatus::Ready; }

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::AcceptingRoleExecutor::Initialize()
{
    // There is no initialization messages needed from the acceptor strategy. 
    return { m_tracker.GetStatus(), {} };
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::AcceptingRoleExecutor::Synchronize(ReadableView buffer)
{
    switch (m_tracker.GetStage()) {
        case Stage::CipherSuiteSelection: { return ExecuteCipherSuiteSelectionStage(buffer); }
        case Stage::KeyExchange: { return ExecuteKeyExchangeStage(buffer); }
        // It is an error to be called in all other synchronization stages. 
        case Stage::Synchronized: {
            m_tracker.SetError();
            return { m_tracker.GetStatus(), {} };
        }
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::AcceptingRoleExecutor::ExecuteCipherSuiteSelectionStage(
    ReadableView request)
{
    auto optResponse = OnCipherSuiteSelectionRequest(request);

    if (!optResponse) {
        m_tracker.SetError();
        return { m_tracker.GetStatus(), {} };
    }

    m_tracker.SetStage(Stage::KeyExchange, request, std::span{ *optResponse });

    return { m_tracker.GetStatus(), std::move(*optResponse) };
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PackageSynchronizer::AcceptingRoleExecutor::OnCipherSuiteSelectionRequest(ReadableView request)
{
    auto const spSupportedAlgorithms = m_context.get().GetSupportedAlgorithms().lock();
    if (!spSupportedAlgorithms) {
        return {};
    }

    std::optional<CipherSuite> optCipherSuite;

    try {
        constexpr std::size_t maximumSize = [] () {
            std::size_t size = 0;
            // Each algorithm array will have list the number of elements expected and number of total bytes used. 
            size += 3 * sizeof(std::uint8_t) + sizeof(std::uint16_t);
            // Each algorithm array may have the maximum number of elements and each element's size will may reach the 
            // maximum allowed, each item will be preceeded by the it's size. 
            size += 3 * ((SupportedConfidentialityLevelSize * MaximumSupportedAlgorithmElements) * (MaximumSupportedAlgorithmNameSize + 1));
            return size;
        }();

        if (request.size() > maximumSize) {
            return {}; // If the packed buffer exceeds the maximum expected size, an error has occurred. 
        }

        // First determine the size of the packed data received from the peer. 
        auto begin = request.cbegin();
        auto const end = request.cend();
        
        auto const [keyAgreementLevel, keyAgreementName] = local::ParseAndMatchSuiteComponent(begin, end, *spSupportedAlgorithms,
            [] (auto const& values, auto const& algorithms) -> std::optional<std::string> {
                auto const& agreements = algorithms.GetKeyAgreements();
                for (auto const& agreement : agreements) {
                    if (std::ranges::contains(values, agreement)) {
                        return agreement;
                    }
                }
                return {};
            });

        if (keyAgreementLevel == ConfidentialityLevel::Unknown) {
            return {};
        }

        auto const [cipherLevel, cipherName] = local::ParseAndMatchSuiteComponent(begin, end, *spSupportedAlgorithms,
            [] (auto const& values, auto const& algorithms) -> std::optional<std::string> {
                auto const& ciphers = algorithms.GetCiphers();
                for (auto const& cipher : ciphers) {
                    if (std::ranges::contains(values, cipher)) {
                        return cipher;
                    }
                }
                return {};
            });

        if (cipherLevel == ConfidentialityLevel::Unknown) {
            return {};
        }

        auto const [hashFunctionLevel, hashFunctionName] = local::ParseAndMatchSuiteComponent(begin, end, *spSupportedAlgorithms,
            [] (auto const& values, auto const& algorithms) -> std::optional<std::string> {
                auto const& hashes = algorithms.GetHashFunctions();
                for (auto const& hash : hashes) {
                    if (std::ranges::contains(values, hash)) {
                        return hash;
                    }
                }
                return {};
            });

        if (hashFunctionLevel == ConfidentialityLevel::Unknown) {
            return {};
        }

        // Determine the lowest confidentiality that this cipher suite may be used for. Only one component needs to
        // be a lower level to degrade the entire suite. 
        ConfidentialityLevel lowestLevel = keyAgreementLevel;
        if (cipherLevel < lowestLevel) { lowestLevel = cipherLevel; }
        if (hashFunctionLevel < lowestLevel) { lowestLevel = hashFunctionLevel; }

        if (lowestLevel == ConfidentialityLevel::Unknown) {
            return {}; // If no valid shared suite could be found then we cannot negotiate with the peer. 
        }

        optCipherSuite = CipherSuite{ lowestLevel, keyAgreementName, cipherName, hashFunctionName };
    } catch (...) {
        return {}; // If an exception occurred while parsing, an error has occurred. 
    }

    m_upModel = local::CreateSynchronizerModel(optCipherSuite->GetKeyAgreementName());
    if (!m_upModel) {
        return {}; // If we have failed to make a synchronizer model, an error has occurred. 
    }

    auto optPublicKey = m_upModel->SetupKeyExchange(*optCipherSuite);
    if (!optPublicKey) {
        return {}; // If the model indicates it could not setup the key exchange, an error has occurred. 
    }

    auto const& keyAgreementName = optCipherSuite->GetKeyAgreementName();
    auto const& cipherName = optCipherSuite->GetCipherName();
    auto const& hashFunctionName = optCipherSuite->GetHashFunctionName();

    std::size_t const responseSize = [&] () -> std::size_t {
        std::size_t size = 0;
        size += sizeof(std::uint16_t); // 2 bytes for destination type
        size += keyAgreementName.size(); // N bytes for message protocol type 
        size += sizeof(std::uint16_t); // 2 bytes for destination type
        size += cipherName.size(); // N bytes for message protocol type 
        size += sizeof(std::uint16_t); // 2 bytes for destination type
        size += hashFunctionName.size(); // N bytes for message protocol type 
        size += sizeof(std::uint32_t); // 2 bytes for destination type
        size += optPublicKey->GetSize(); // N bytes for message protocol type 
        size += sizeof(std::uint16_t); // 2 bytes for destination type
        size += m_context.get().GetSaltSize(); // N bytes for message protocol type 
        return size;
    }();

    Buffer response;
    response.reserve(responseSize);

    PackUtils::PackChunk<std::uint16_t>(keyAgreementName, response); // Pack the name of the key agreement.
    PackUtils::PackChunk<std::uint16_t>(cipherName, response); // Pack the name of the cipher.
    PackUtils::PackChunk<std::uint16_t>(hashFunctionName, response); // Pack the name of the hash function. 
    PackUtils::PackChunk<std::uint32_t>(optPublicKey->GetData(), response);

    {
        auto const& salt = m_context.get().SetupKeyShare(std::move(optCipherSuite), std::move(*optPublicKey));
        PackUtils::PackChunk<std::uint16_t>(salt.GetData(), response);
    }

    return response;
}

//----------------------------------------------------------------------------------------------------------------------

Security::SynchronizationResult Security::PackageSynchronizer::AcceptingRoleExecutor::ExecuteKeyExchangeStage(
    ReadableView request)
{
    // Handle the initiator's initialization request. The post conditions for this handling 
    // include generating the session keys and updating the transaction's plaintext data. 
    auto optResponse = OnKeyExchangeRequest(request);

    // Set an error if the request could not be handled. 
    if (!optResponse) {
        m_tracker.SetError();
        return { m_tracker.GetStatus(), {} };
    }

    if (m_tracker.VerifyTransaction(request) != VerificationStatus::Success) {
        m_tracker.SetError();
        return { m_tracker.GetStatus(), {} };
    }

    // Sign the entire transaction and inject the signature into the response such that the initiator can verify. 
    if (!m_tracker.SignTransaction(*optResponse)) {
        m_tracker.SetError();
        return {};
    }

    m_tracker.Finalize(Stage::Synchronized);

    return { m_tracker.GetStatus(), *optResponse };
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::PackageSynchronizer::AcceptingRoleExecutor::OnKeyExchangeRequest(ReadableView request)
{
    if (!m_upModel) {
        return {}; // If the key exchange model has not been created, an error has occurred. 
    }

    constexpr std::size_t minimumRequestSize = [] () -> std::size_t {
        std::size_t size = 0;
        size += sizeof(std::uint16_t); // 1 byte for the size of the key agreement name
        size += sizeof(std::uint16_t); // 1 byte for the size of the cipher name
        size += sizeof(std::uint16_t); // 1 byte for the size of the hash function name
        return size;
    }();

    std::size_t const maximumRequestSize = [&] () -> std::size_t {
        std::size_t size = 0;
        size += sizeof(std::uint16_t); // 1 byte for the size of the key agreement name
        size += MaximumSupportedAlgorithmNameSize; // N bytes for the maximum size of the key agreement name
        size += sizeof(std::uint16_t); // 1 byte for the size of the cipher name
        size += MaximumSupportedAlgorithmNameSize; // N bytes for the maximum size of the cipher name
        size += sizeof(std::uint16_t); // 1 byte for the size of the hash function name
        size += MaximumSupportedAlgorithmNameSize; // N bytes for the maximum size of the hash function name
        size += sizeof(std::uint32_t); // 4 bytes for the size of the hash function name
        size += MaximumExpectedPublicKeySize; // N bytes for the maximum size of the hash function name
        size += sizeof(std::uint16_t); // 2 bytes for the size of the hash function name
        size += MaximumExpectedSaltSize; // N bytes for the maximum size of the hash function name
        size += m_upModel->GetSupplementalDataSize(); // N bytes for the supplemental data sent by the key exchange model. 
        size += m_context.get().GetVerificationDataSize(); // N bytes for the verification data. 
        size += m_context.get().GetSignatureSize(); // N bytes for the transaction signature. 
        return size;
    }();

    if (request.size() < minimumRequestSize || request.size() > maximumRequestSize) {
        return {}; // If the response size is not within the expected bounds, an error has occurred. 
    }

    auto const& optCipherSuite = m_context.get().GetCipherSuite();

    auto begin = request.begin();
	auto const end = request.end();

    std::string const keyAgreement = [&begin, &end] () -> std::string {
		std::uint16_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return ""; }

        std::string value;
		if (!PackUtils::UnpackChunk(begin, end, value, size)) { return ""; }

        return value;
    }();

    if (keyAgreement != optCipherSuite->GetKeyAgreementName()) {
        return {}; // If the key agreement name is empty after parsing, an error has occurred. 
    }

    std::string const cipher = [&begin, &end] () -> std::string {
		std::uint16_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return ""; }

        std::string value;
		if (!PackUtils::UnpackChunk(begin, end, value, size)) { return ""; }

        return value;
    }();

    if (cipher != optCipherSuite->GetCipherName()) {
        return {}; // If the key agreement name is empty after parsing, an error has occurred. 
    }

    std::string const hashFunction = [&begin, &end] () -> std::string {
		std::uint16_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return ""; }

        std::string value;
		if (!PackUtils::UnpackChunk(begin, end, value, size)) { return ""; }

        return value;
    }();

    if (hashFunction != optCipherSuite->GetHashFunctionName()) {
        return {}; // If the key agreement name is empty after parsing, an error has occurred. 
    }

    auto publicKey = [&begin, &end, &context = m_context] () -> PublicKey {
        std::uint32_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return {}; }

        std::size_t const expected = context.get().GetPublicKeySize();
        if (size != expected) { return {}; }

        Buffer buffer;
        if (!PackUtils::UnpackChunk(begin, end, buffer, size)) { return {}; }

        return PublicKey{ std::move(buffer) };
    }();

    if (publicKey.IsEmpty()) {
        return {}; // If we failed to parse a public key from the buffer, an error has occurred. 
    }

    auto const salt = [&begin, &end, &context = m_context] () -> Salt {
        std::uint16_t size = 0;
        if (!PackUtils::UnpackChunk(begin, end, size)) { return {}; }

        std::size_t const expected = context.get().GetSaltSize();
        if (size != expected) { return {}; }

        Buffer buffer;
        if (!PackUtils::UnpackChunk(begin, end, buffer, size)) { return {}; }

        return Salt{ std::move(buffer) };
    }();

    if (salt.IsEmpty()) {
        return {}; // If we failed to parse salt from the buffer, an error has occurred. 
    }

    std::optional<SharedSecret> optSharedSecret;
    if (m_upModel->HasSupplementalData()) {
        auto const supplementalData = [&begin, &end, &upModel = m_upModel] () -> SupplementalData {
            Buffer buffer;
            if (!PackUtils::UnpackChunk(begin, end, buffer, upModel->GetSupplementalDataSize())) { return {}; }
            return buffer;
        }();

        if (supplementalData.GetSize() != m_upModel->GetSupplementalDataSize()) {
            return {}; // If the size of the parsed supplemental data is different than what is expected, an error has occurred. 
        }

        optSharedSecret = m_upModel->ComputeSharedSecret(supplementalData);
        if (!optSharedSecret) {
            return {}; // If the model failed to compute a shared secret using the peer's public key, an error has occurred. 
        }
    } else {
        auto optResult = m_upModel->ComputeSharedSecret(publicKey);
        if (!optResult) {
            return {}; // If the model failed to compute a shared secret using the peer's public key, an error has occurred. 
        }
        optSharedSecret = std::move(optResult->first);
    }

    if (!m_context.get().SetPeerPublicKeyAndSalt(std::move(publicKey), salt)) {
        return {}; // If are unable to set the peer's public key and salt, an error has occurred. 
    }

    // Note: After this point it is no longer valid to use the key store as it has been moved into the 
    // generated cipher package. 
    auto const optVerificationData = m_context.get().GenerateSessionKeys(std::move(*optSharedSecret));
    if (!optVerificationData) {
        return {}; // If the we failed to generate the session keys, an error has occureed. 
    }

    if (m_context.get().VerifyKeyShare(ReadableView{ begin, m_context.get().GetVerificationDataSize() }) != VerificationStatus::Success) {
        return {}; // If we are unable to verify the key share, an error has occurred. 
    }

    return optVerificationData;
}

//----------------------------------------------------------------------------------------------------------------------
// } Security::PackageSynchronizer::AcceptingRoleExecutor
//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Security::Detail::ISynchronizerModel> local::CreateSynchronizerModel(std::string const& keyAgreement)
{
    using SynchronizationModelGenerator = std::function<
        std::unique_ptr<Detail::ISynchronizerModel>(std::string const& keyAgreement)>;

    static std::unordered_map<std::string, SynchronizationModelGenerator> const mapping {
        { 
            "ffdhe",
            [] (auto const& keyAgreement) -> std::unique_ptr<Detail::ISynchronizerModel> { 
                if (!Classical::FiniteFieldDiffieHellmanModel::IsKeyAgreementSupported(keyAgreement)) {
                    return nullptr;
                }
                return std::make_unique<Classical::FiniteFieldDiffieHellmanModel>();
            }
        },
        { 
            "ecdh",
            [] (auto const& keyAgreement) -> std::unique_ptr<Detail::ISynchronizerModel> { 
                if (!Classical::EllipticCurveDiffieHellmanModel::IsKeyAgreementSupported(keyAgreement)) {
                    return nullptr;
                }
                return std::make_unique<Classical::EllipticCurveDiffieHellmanModel>(); 
            }
        },
        { 
            "kem",
            [] (auto const& keyAgreement) -> std::unique_ptr<Detail::ISynchronizerModel> { 
                if (!Quantum::KeyEncapsulationModel::IsKeyAgreementSupported(keyAgreement)) {
                    return nullptr;
                }
                return std::make_unique<Quantum::KeyEncapsulationModel>();
            }
        },
    };

    constexpr char AlgorithmNameDelimiter = '-';

    std::size_t const position = keyAgreement.find(AlgorithmNameDelimiter);
    std::string const modelName = (position != std::string::npos) ? keyAgreement.substr(0, position) : keyAgreement;

    if (auto const itr = mapping.find(modelName); itr != mapping.end()) {
        auto const& [key, generator] = *itr;
        return generator(keyAgreement);
    }

    return nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

local::SuiteComponent local::ParseAndMatchSuiteComponent(
    ReadableView::const_iterator& begin,
    ReadableView::const_iterator const& end,
    Configuration::Options::SupportedAlgorithms const& supportedAlgorithms,
    ComponentPredicate const& predicate)
{
    using enum ConfidentialityLevel;

    std::uint16_t elements = 0;
    std::uint16_t bytesUsed = 0;

    // If we fail to unpack the size component of the packed data, an error has occurred. 
    if (!PackUtils::UnpackChunk(begin, end, elements) || elements > SupportedConfidentialityLevelSize * MaximumSupportedAlgorithmElements) { 
        return {};
    }

    // If we fail to unpack the size component of the packed data, an error has occurred. 
    if (!PackUtils::UnpackChunk(begin, end, bytesUsed) || bytesUsed > ((SupportedConfidentialityLevelSize * MaximumSupportedAlgorithmElements) * (MaximumSupportedAlgorithmNameSize + 1))) { 
        return {};
    }

    if (std::cmp_less(std::distance(begin, end), bytesUsed)) {
        return {};
    }

    std::vector<std::string> values;
    std::uint16_t bytesRead = 0;
    for (std::uint16_t element = 0; element != elements; ++element) {
		std::uint16_t size = 0;
        std::string value;
        if (!PackUtils::UnpackChunk(begin, end, size) || size > MaximumSupportedAlgorithmNameSize) { return {}; }
        if (!PackUtils::UnpackChunk(begin, end, value, size)) { return {}; }
        bytesRead += sizeof(size) + value.size();
        values.emplace_back(std::move(value));
    }

    SuiteComponent component = { Unknown, "" };
    supportedAlgorithms.ForEachSupportedAlgorithm([&] (ConfidentialityLevel level, Configuration::Options::Algorithms const& algorithms) {
        if (auto optMatched = predicate(values, algorithms); optMatched) {
            component = { level, std::move(*optMatched) };
            return CallbackIteration::Stop;
        }
        return CallbackIteration::Continue;
    });

    return component;
}

//----------------------------------------------------------------------------------------------------------------------
