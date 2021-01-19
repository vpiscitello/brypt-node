//------------------------------------------------------------------------------------------------
// File: SecurityStrategy.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Security/SecurityTypes.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
//------------------------------------------------------------------------------------------------

class ISecurityStrategy {
public:
    virtual ~ISecurityStrategy() = default;

    [[nodiscard]] virtual Security::Strategy GetStrategyType() const = 0;
    [[nodiscard]] virtual Security::Role GetRoleType() const = 0;
    [[nodiscard]] virtual Security::Context GetContextType() const = 0;
    [[nodiscard]] virtual std::size_t GetSignatureSize() const = 0;

    [[nodiscard]] virtual std::uint32_t GetSynchronizationStages() const = 0;
    [[nodiscard]] virtual Security::SynchronizationStatus GetSynchronizationStatus() const = 0;
    [[nodiscard]] virtual Security::SynchronizationResult PrepareSynchronization() = 0;
    [[nodiscard]] virtual Security::SynchronizationResult Synchronize(
        Security::ReadableView buffer) = 0;

    [[nodiscard]] virtual Security::OptionalBuffer Encrypt(
        Security::ReadableView, std::uint64_t nonce) const = 0;
    [[nodiscard]] virtual Security::OptionalBuffer Decrypt(
        Security::ReadableView buffer, std::uint64_t nonce) const = 0;

    [[nodiscard]] virtual std::int32_t Sign(Security::Buffer& buffer) const = 0;
    [[nodiscard]] virtual Security::VerificationStatus Verify(
        Security::ReadableView buffer) const = 0;

private: 
    [[nodiscard]] virtual std::int32_t Sign(
        Security::ReadableView source, Security::Buffer& destination) const = 0;

    [[nodiscard]] virtual Security::OptionalBuffer GenerateSignature(
        Security::ReadableView key, Security::ReadableView data) const = 0;
};

//------------------------------------------------------------------------------------------------
