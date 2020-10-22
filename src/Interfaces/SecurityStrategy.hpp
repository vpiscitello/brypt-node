//------------------------------------------------------------------------------------------------
// File: SecurityStrategy.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Security/SecurityTypes.hpp"
#include "../Components/Security/SecurityDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
//------------------------------------------------------------------------------------------------

class ISecurityStrategy {
public:
    virtual ~ISecurityStrategy() = default;

    virtual Security::Strategy GetStrategyType() const = 0;
    virtual Security::Role GetRole() const = 0;

    virtual std::uint32_t SynchronizationStages() const = 0;
    virtual Security::SynchronizationStatus SynchronizationStatus() const = 0;
    virtual Security::SynchronizationResult Synchronize(Security::Buffer const& buffer) = 0;

    virtual Security::OptionalBuffer Encrypt(
        Security::Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const = 0;
    virtual Security::OptionalBuffer Decrypt(
        Security::Buffer const& buffer, std::uint32_t size, std::uint64_t nonce) const = 0;

    virtual std::uint32_t Sign(Security::Buffer& buffer) const = 0;
    virtual Security::VerificationStatus Verify(Security::Buffer const& buffer) const = 0;

private: 
    virtual Security::OptionalBuffer GenerateSignature(
        std::uint8_t const* pKey,
        std::uint32_t keySize,
        std::uint8_t const* pData,
        std::uint32_t dataSize) const = 0;

};

//------------------------------------------------------------------------------------------------
