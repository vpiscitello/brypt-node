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
#include <openssl/evp.h>
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

using TransactionHasher = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

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
    CSynchronizationTracker();

    SynchronizationStatus GetStatus() const;
    void SetError();
    void AddTransactionData(Buffer const& buffer);
    VerificationStatus VerifyTransaction(Buffer const& buffer);
    void ResetState();

    template <typename EnumType>
    EnumType GetStage() const;

    template<typename EnumType>
    void SetStage(EnumType);

    template<typename EnumType>
    void FinalizeTransaction(EnumType type);
    
private:
    SynchronizationStatus m_status;
    std::uint8_t m_stage;
    TransactionHasher m_upTransactionHasher;

};

//------------------------------------------------------------------------------------------------

class Security::PQNISTL3::CStrategy : public ISecurityStrategy {
public:
    constexpr static Strategy Type = Strategy::PQNISTLevelThree;

    enum class InitiatorSynchronizationStage : std::uint8_t { 
        Invalid, Initialization, Decapsulation, Complete };
    enum class AcceptorSynchronizationStage : std::uint8_t { 
        Invalid, Initialization, Encapsulation, Verification, Complete };

    constexpr static std::uint32_t InitiatorSynchronizationStages = 2; 
    constexpr static std::uint32_t AcceptorSynchronizationStages = 3; 
    
    constexpr static std::uint32_t PrincipalRandomLength = 32;
    constexpr static std::uint32_t SignatureLength = 48;

    CStrategy(Role role, Context context);

    CStrategy(CStrategy&& other) = delete;
    CStrategy(CStrategy const& other) = delete;
    CStrategy& operator=(CStrategy const& other) = delete;

    // ISecurityStrategy {
    virtual Strategy GetStrategyType() const override;
    virtual Role GetRole() const override;

    virtual std::uint32_t SynchronizationStages() const override;
    virtual SynchronizationStatus GetSynchronizationStatus() const override;
    virtual SynchronizationResult Synchronize(Buffer const& buffer) override;

    virtual OptionalBuffer Encrypt(
        Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const override;

    virtual OptionalBuffer Decrypt(
        Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const override;

    virtual std::uint32_t Sign(Buffer& buffer) const override;

    virtual VerificationStatus Verify(Buffer const& buffer) const override;
    // } ISecurityStrategy

    static void InitializeApplicationContext();

    std::weak_ptr<CContext> GetSessionContext() const;
    std::uint32_t GetPublicKeySize() const;

private:
    constexpr static std::string_view KeyEncapsulationSchme = "NTRU-HPS-2048-677";
    constexpr static std::string_view EncryptionScheme = "AES-256-CTR";
    constexpr static std::string_view MessageAuthenticationScheme = "SHA384";

    // ISecurityStrategy {
    virtual OptionalBuffer GenerateSignature(
        std::uint8_t const* pKey,
        std::uint32_t keySize,
        std::uint8_t const* pData,
        std::uint32_t dataSize) const override;
    // } ISecurityStrategy
    
    SynchronizationResult HandleInitiatorSynchronization(Buffer const& buffer);
    SynchronizationResult HandleInitiatorInitialization(Buffer const& buffer);
    SynchronizationResult HandleInitiatorDecapsulation(Buffer const& buffer);

    SynchronizationResult HandleAcceptorSynchronization(Buffer const& buffer);
    SynchronizationResult HandleAcceptorInitialization(Buffer const& buffer);
    SynchronizationResult HandleAcceptorEncapsulation(Buffer const& buffer);
    SynchronizationResult HandleAcceptorVerification(Buffer const& buffer);

    OptionalBuffer EncapsulateSharedSecret();
    bool DecapsulateSharedSecret(Buffer const& encapsulation);

    Role m_role;

    CSynchronizationTracker m_synchronization;

    static std::shared_ptr<CContext> m_spSharedContext;

    std::shared_ptr<CContext> m_spSessionContext;

    oqs::KeyEncapsulation m_kem;
    Security::CKeyStore m_store;

};

//------------------------------------------------------------------------------------------------