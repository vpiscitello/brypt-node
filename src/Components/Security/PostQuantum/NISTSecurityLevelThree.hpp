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
#include <shared_mutex>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Security {
namespace PQNISTL3 {
//------------------------------------------------------------------------------------------------

class CContext;
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
    using EncapsulationCallback = std::function<void(Security::Buffer&&, Security::Buffer&&)>;

    CContext(std::string_view kem);

    std::uint32_t GetPublicKeySize() const;
    Security::Buffer GetPublicKey() const;
    std::uint32_t GetPublicKey(Security::Buffer& buffer) const;
    bool GenerateEncapsulatedSecret(
        Security::Buffer const& publicKey, EncapsulationCallback const& callback) const;
    bool DecapsulateSecret(
        Security::Buffer const& encapsulation, Security::Buffer& decapsulation) const;

    CContext(CContext const&) = delete;
    CContext(CContext&&) = delete;
    void operator=(CContext const&) = delete;

private:
    mutable std::shared_mutex m_kemMutex;
    oqs::KeyEncapsulation m_kem;

    mutable std::shared_mutex m_publicKeyMutex;
    Security::Buffer m_publicKey;

};

//------------------------------------------------------------------------------------------------

class Security::PQNISTL3::CStrategy : public ISecurityStrategy {
public:
    constexpr static Security::Strategy Type = Security::Strategy::PQNISTLevelThree;

    enum class InitiatorSynchronizationStage : std::uint8_t { 
        Invalid, Initialization, Decapsulation, Complete };
    enum class AcceptorSynchronizationStage : std::uint8_t { 
        Invalid, Initialization, Encapsulation, Verification, Complete };

    constexpr static std::uint32_t InitiatorSynchronizationStages = 2; 
    constexpr static std::uint32_t AcceptorSynchronizationStages = 3; 
    
    constexpr static std::uint32_t PrincipalRandomLength = 32;
    constexpr static std::uint32_t SignatureLength = 48;

    explicit CStrategy(Context context);

    CStrategy(CStrategy&& other) = delete;
    CStrategy(CStrategy const& other) = delete;
    CStrategy& operator=(CStrategy const& other) = delete;

    // ISecurityStrategy {
    virtual Security::Strategy GetStrategyType() const override;
    virtual void SetRole(Security::Role role) override;

    virtual std::uint32_t SynchronizationStages() const override;
    virtual Security::SynchronizationStatus SynchronizationStatus() const override;
    virtual Security::SynchronizationResult Synchronize(Security::Buffer const& buffer) override;

    virtual Security::OptionalBuffer Encrypt(
        Security::Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const override;

    virtual Security::OptionalBuffer Decrypt(
        Security::Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const override;

    virtual std::uint32_t Sign(Security::Buffer& buffer) const override;

    virtual Security::VerificationStatus Verify(Security::Buffer const& buffer) const override;
    // } ISecurityStrategy

    static void InitializeApplicationContext();

    std::weak_ptr<CContext> GetSessionContext() const;
    std::uint32_t GetPublicKeySize() const;

private:
    constexpr static std::string_view KeyEncapsulationSchme = "NTRU-HPS-2048-677";
    constexpr static std::string_view EncryptionScheme = "AES-256-CTR";
    constexpr static std::string_view MessageAuthenticationScheme = "SHA384";

    // ISecurityStrategy {
    virtual Security::OptionalBuffer GenerateSignature(
        std::uint8_t const* pKey,
        std::uint32_t keySize,
        std::uint8_t const* pData,
        std::uint32_t dataSize) const override;
    // } ISecurityStrategy
    
    Security::SynchronizationResult HandleInitiatorSynchronization(
        Security::Buffer const& buffer);
    Security::SynchronizationResult HandleInitiatorInitialization(
        Security::Buffer const& buffer);
    Security::SynchronizationResult HandleInitiatorDecapsulation(
        Security::Buffer const& buffer);

    Security::SynchronizationResult HandleAcceptorSynchronization(
        Security::Buffer const& buffer);
    Security::SynchronizationResult HandleAcceptorInitialization(
        Security::Buffer const& buffer);
    Security::SynchronizationResult HandleAcceptorEncapsulation(
        Security::Buffer const& buffer);
    Security::SynchronizationResult HandleAcceptorVerification(
        Security::Buffer const& buffer);

    Security::OptionalBuffer EncapsulateSharedSecret();
    bool DecapsulateSharedSecret(Security::Buffer const& encapsulation);

    Security::Role m_role;

    struct TSynchronication
    {
        std::uint8_t stage;
        Security::SynchronizationStatus status;
    } m_synchronization;

    static std::shared_ptr<CContext> m_spSharedContext;

    std::shared_ptr<CContext> m_spSessionContext;

    oqs::KeyEncapsulation m_kem;
    Security::CKeyStore m_store;

};

//------------------------------------------------------------------------------------------------
