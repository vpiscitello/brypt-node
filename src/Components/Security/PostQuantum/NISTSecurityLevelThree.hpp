//------------------------------------------------------------------------------------------------
// File: NISTSecurityLevelThree.hpp
// Description: Contains the SecurityStrategy for securing communications using Post Quantum 
// methods theoretically providing NIST Security Level Three protections. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Security/KeyStore.hpp"
#include "Components/Security/SecurityTypes.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Interfaces/SecurityStrategy.hpp"
//------------------------------------------------------------------------------------------------
#include <oqscpp/oqs_cpp.h>
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Security {
namespace PQNISTL3 {
//------------------------------------------------------------------------------------------------

class Context;
class SynchronizationTracker;
class Strategy;

//------------------------------------------------------------------------------------------------
} // PQNISTL3 namespace
} // Security namespace
//------------------------------------------------------------------------------------------------

class Security::PQNISTL3::Context
{
public:
    // A callback providing the caller the encapsulation and the shared secret. The caller may 
    // take ownsership of both of these items. 
    using EncapsulationCallback = std::function<bool(Buffer&&, Buffer&&)>;

    constexpr static std::size_t PublicKeySize = 930;
    constexpr static std::size_t EncapsulationSize = 930;

    Context(std::string_view kem);

    [[nodiscard]] std::size_t GetPublicKeySize() const;
    [[nodiscard]] Buffer GetPublicKey() const;
    [[nodiscard]] std::size_t GetPublicKey(Buffer& buffer) const;

    [[nodiscard]] bool GenerateEncapsulatedSecret(
        Buffer const& publicKey, EncapsulationCallback const& callback) const;
    [[nodiscard]] bool DecapsulateSecret(
        Buffer const& encapsulation, Buffer& decapsulation) const;

    Context(Context const&) = delete;
    Context(Context&&) = delete;
    void operator=(Context const&) = delete;

private:
    mutable std::shared_mutex m_kemMutex;
    oqs::KeyEncapsulation m_kem;

    mutable std::shared_mutex m_publicKeyMutex;
    Buffer m_publicKey;
};

//------------------------------------------------------------------------------------------------

class Security::PQNISTL3::SynchronizationTracker
{
public:
    using TransactionSignator = std::function<std::size_t(Buffer const&, Buffer&)>;
    using TransactionVerifier = std::function<VerificationStatus(Buffer const& buffer)>;

    SynchronizationTracker();

    SynchronizationStatus GetStatus() const;
    void SetError();

    template <typename EnumType>
    [[nodiscard]] EnumType GetStage() const;

    template<typename EnumType>
    void SetStage(EnumType stage);

    void SetSignator(TransactionSignator const& signator);
    void SetVerifier(TransactionVerifier const& verifier);
    void UpdateTransaction(ReadableView buffer);
    [[nodiscard]] bool SignTransaction(Buffer& message);
    [[nodiscard]] VerificationStatus VerifyTransaction();

    template<typename EnumType>
    void Finalize(EnumType stage);

    [[nodiscard]] bool ResetState();

private:
    SynchronizationStatus m_status;
    std::uint8_t m_stage;
    
    Buffer m_transaction;
    TransactionSignator m_signator;
    TransactionVerifier m_verifier;
};

//------------------------------------------------------------------------------------------------

class Security::PQNISTL3::Strategy : public ISecurityStrategy {
public:
    constexpr static Security::Strategy Type = Security::Strategy::PQNISTL3;

    constexpr static std::string_view KeyEncapsulationSchme = "NTRU-HPS-2048-677";
    constexpr static std::string_view EncryptionScheme = "AES-256-CTR";
    constexpr static std::string_view MessageAuthenticationScheme = "SHA384";

    enum class InitiatorStage : std::uint8_t { Initialization, Complete };
    enum class AcceptorStage : std::uint8_t { Initialization, Verification, Complete };
    constexpr static std::uint32_t InitiatorStages = 1; 
    constexpr static std::uint32_t AcceptorStages = 2; 
    
    constexpr static std::size_t PrincipalRandomSize = 32;
    constexpr static std::size_t SignatureSize = 48;

    Strategy(Role role, Security::Context context);

    Strategy(Strategy&& other) = delete;
    Strategy(Strategy const& other) = delete;
    Strategy& operator=(Strategy const& other) = delete;

    // ISecurityStrategy {
    [[nodiscard]] virtual Security::Strategy GetStrategyType() const override;
    [[nodiscard]] virtual Role GetRoleType() const override;
    [[nodiscard]] virtual Security::Context GetContextType() const override;
    [[nodiscard]] virtual std::size_t GetSignatureSize() const override;

    [[nodiscard]] virtual std::uint32_t GetSynchronizationStages() const override;
    [[nodiscard]] virtual SynchronizationStatus GetSynchronizationStatus() const override;
    [[nodiscard]] virtual SynchronizationResult PrepareSynchronization() override;
    [[nodiscard]] virtual SynchronizationResult Synchronize(ReadableView buffer) override;

    [[nodiscard]] virtual OptionalBuffer Encrypt(
        ReadableView buffer, std::uint64_t nonce) const override;
    [[nodiscard]] virtual OptionalBuffer Decrypt(
        ReadableView buffer, std::uint64_t nonce) const override;

    [[nodiscard]] virtual std::int32_t Sign(Buffer& buffer) const override;
    [[nodiscard]] virtual VerificationStatus Verify(ReadableView buffer) const override;
    // } ISecurityStrategy

    static void InitializeApplicationContext();
    static void ShutdownApplicationContext();

    [[nodiscard]] std::weak_ptr<Context> GetSessionContext() const;
    [[nodiscard]] std::size_t GetPublicKeySize() const;

    [[nodiscard]] OptionalBuffer EncapsulateSharedSecret();
    [[nodiscard]] bool DecapsulateSharedSecret(Buffer const& encapsulation);
    [[nodiscard]] OptionalBuffer GenerateVerficationData();
    [[nodiscard]] VerificationStatus VerifyKeyShare(Buffer const& buffer) const;

private:
    // ISecurityStrategy {
    [[nodiscard]] virtual std::int32_t Sign(
        ReadableView source, Security::Buffer& destination) const override;
    [[nodiscard]] virtual OptionalBuffer GenerateSignature(
        ReadableView key, ReadableView data) const override;
    // } ISecurityStrategy
    
    [[nodiscard]] SynchronizationResult HandleInitiatorSynchronization(ReadableView buffer);
    [[nodiscard]] SynchronizationResult HandleInitiatorInitialization(ReadableView buffer);

    [[nodiscard]] SynchronizationResult HandleAcceptorSynchronization(ReadableView buffer);
    [[nodiscard]] SynchronizationResult HandleAcceptorInitialization(ReadableView buffer);
    [[nodiscard]] SynchronizationResult HandleAcceptorVerification(ReadableView buffer);
 
    Role m_role;
    Security::Context m_context;
    SynchronizationTracker m_synchronization;

    static std::shared_ptr<Context> m_spSharedContext;
    std::shared_ptr<Context> m_spSessionContext;

    oqs::KeyEncapsulation m_kem;
    Security::KeyStore m_store;

};

//------------------------------------------------------------------------------------------------
