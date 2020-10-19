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
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

class CPQNISTL3Strategy : public ISecurityStrategy {
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

    CPQNISTL3Strategy();

    CPQNISTL3Strategy(CPQNISTL3Strategy&& other) = delete;
    CPQNISTL3Strategy(CPQNISTL3Strategy const& other) = delete;
    CPQNISTL3Strategy& operator=(CPQNISTL3Strategy const& other) = delete;

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

    oqs::KeyEncapsulation m_kem;
    Security::CKeyStore m_store;

};

//------------------------------------------------------------------------------------------------
