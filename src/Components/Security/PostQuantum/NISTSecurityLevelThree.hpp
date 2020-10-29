//------------------------------------------------------------------------------------------------
// File: NISTSecurityLevelThree.hpp
// Description: Contains the SecurityStrategy for securing communications using Post Quantum 
// methods theoretically providing NIST Security Level Three protections. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../KeyStore.hpp"
#include "../SecurityTypes.hpp"
#include "../SecurityDefinitions.hpp"
#include "../../Interfaces/SecurityStrategy.hpp"
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

class CContext;
class CSynchronizationTracker;

class CStrategy;

//------------------------------------------------------------------------------------------------
} // PQNISTL3 namespace
} // Security namespace
//------------------------------------------------------------------------------------------------

class Security::PQNISTL3::CContext
{
public:
    // A callback providing the caller the encapsulation and the shared secret. The caller may 
    // take ownsership of both of these items. 
    using EncapsulationCallback = std::function<void(Buffer&&, Buffer&&)>;

    constexpr static std::uint32_t PublicKeySize = 930;
    constexpr static std::uint32_t EncapsulationSize = 930;

    CContext(std::string_view kem);

    std::uint32_t GetPublicKeySize() const;
    Buffer GetPublicKey() const;
    std::uint32_t GetPublicKey(Buffer& buffer) const;

    bool GenerateEncapsulatedSecret(
        Buffer const& publicKey, EncapsulationCallback const& callback) const;
    bool DecapsulateSecret(
        Buffer const& encapsulation, Buffer& decapsulation) const;

    CContext(CContext const&) = delete;
    CContext(CContext&&) = delete;
    void operator=(CContext const&) = delete;

private:
    mutable std::shared_mutex m_kemMutex;
    oqs::KeyEncapsulation m_kem;

    mutable std::shared_mutex m_publicKeyMutex;
    Buffer m_publicKey;

};

//------------------------------------------------------------------------------------------------

class Security::PQNISTL3::CSynchronizationTracker
{
public:
    using TransactionSignator = std::function<std::uint32_t(Buffer const&, Buffer&)>;
    using TransactionVerifier = std::function<VerificationStatus(Buffer const& buffer)>;

    CSynchronizationTracker();

    SynchronizationStatus GetStatus() const;
    void SetError();

    template <typename EnumType>
    EnumType GetStage() const;

    template<typename EnumType>
    void SetStage(EnumType stage);

    void SetSignator(TransactionSignator const& signator);
    void SetVerifier(TransactionVerifier const& verifier);
    bool UpdateTransaction(Buffer const& buffer);
    bool SignTransaction(Buffer& message);
    VerificationStatus VerifyTransaction();

    template<typename EnumType>
    void Finalize(EnumType stage);

    bool ResetState();

private:
    SynchronizationStatus m_status;
    std::uint8_t m_stage;
    
    Buffer m_transaction;
    TransactionSignator m_signator;
    TransactionVerifier m_verifier;

};

//------------------------------------------------------------------------------------------------

class Security::PQNISTL3::CStrategy : public ISecurityStrategy {
public:
    constexpr static Strategy Type = Strategy::PQNISTL3;

    constexpr static std::string_view KeyEncapsulationSchme = "NTRU-HPS-2048-677";
    constexpr static std::string_view EncryptionScheme = "AES-256-CTR";
    constexpr static std::string_view MessageAuthenticationScheme = "SHA384";

    enum class InitiatorStage : std::uint8_t { Initialization, Complete };
    enum class AcceptorStage : std::uint8_t { Initialization, Verification, Complete };
    constexpr static std::uint32_t InitiatorStages = 1; 
    constexpr static std::uint32_t AcceptorStages = 2; 
    
    constexpr static std::uint32_t PrincipalRandomSize = 32;
    constexpr static std::uint32_t SignatureSize = 48;

    CStrategy(Role role, Context context);

    CStrategy(CStrategy&& other) = delete;
    CStrategy(CStrategy const& other) = delete;
    CStrategy& operator=(CStrategy const& other) = delete;

    // ISecurityStrategy {
    virtual Strategy GetStrategyType() const override;
    virtual Role GetRoleType() const override;
    virtual Context GetContextType() const override;

    virtual std::uint32_t GetSynchronizationStages() const override;
    virtual SynchronizationStatus GetSynchronizationStatus() const override;
    virtual SynchronizationResult PrepareSynchronization() override;
    virtual SynchronizationResult Synchronize(Buffer const& buffer) override;

    virtual OptionalBuffer Encrypt(
        Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const override;

    virtual OptionalBuffer Decrypt(
        Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const override;

    virtual std::uint32_t Sign(Buffer& buffer) const override;

    virtual VerificationStatus Verify(Buffer const& buffer) const override;
    // } ISecurityStrategy

    static void InitializeApplicationContext();
    static void ShutdownApplicationContext();

    std::weak_ptr<CContext> GetSessionContext() const;
    std::uint32_t GetPublicKeySize() const;

    OptionalBuffer EncapsulateSharedSecret();
    bool DecapsulateSharedSecret(Buffer const& encapsulation);
    OptionalBuffer GenerateVerficationData();
    VerificationStatus VerifyKeyShare(Buffer const& buffer) const;

private:
    // ISecurityStrategy {
    virtual std::uint32_t Sign(
        Security::Buffer const& source, Security::Buffer& destination) const override;

    virtual OptionalBuffer GenerateSignature(
        std::uint8_t const* pKey,
        std::uint32_t keySize,
        std::uint8_t const* pData,
        std::uint32_t dataSize) const override;
    // } ISecurityStrategy
    
    SynchronizationResult HandleInitiatorSynchronization(Buffer const& buffer);
    SynchronizationResult HandleInitiatorInitialization(Buffer const& buffer);

    SynchronizationResult HandleAcceptorSynchronization(Buffer const& buffer);
    SynchronizationResult HandleAcceptorInitialization(Buffer const& buffer);
    SynchronizationResult HandleAcceptorVerification(Buffer const& buffer);

    Role m_role;
    Context m_context;
    CSynchronizationTracker m_synchronization;

    static std::shared_ptr<CContext> m_spSharedContext;
    std::shared_ptr<CContext> m_spSessionContext;

    oqs::KeyEncapsulation m_kem;
    Security::CKeyStore m_store;

};

//------------------------------------------------------------------------------------------------
