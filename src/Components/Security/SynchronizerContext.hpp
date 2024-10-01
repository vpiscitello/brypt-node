//---------------------------------------------------------------------------------------------------------------------
// File: SynchronizerContext.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "CipherPackage.hpp"
#include "KeyStore.hpp"
#include "SecureBuffer.hpp"
#include "SecurityDefinitions.hpp"
#include "SecurityTypes.hpp"
#include "SecurityUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------

namespace Node { class ServiceProvider; }
namespace Configuration::Options { class SupportedAlgorithms; }
namespace Security { class CipherService; }
namespace Security::Detail { class SynchronizerContext; class ISynchronizerModel; }

//----------------------------------------------------------------------------------------------------------------------

class Security::Detail::SynchronizerContext
{
public:
    using OptionalPublicKeyAndSalt = std::optional<std::pair<ReadableView, ReadableView>>;

    SynchronizerContext(ExchangeRole role, std::weak_ptr<Configuration::Options::SupportedAlgorithms> const& wpSupportedAlgorithms);

    SynchronizerContext(SynchronizerContext&& other) noexcept;
    SynchronizerContext& operator=(SynchronizerContext&& other) noexcept;

    SynchronizerContext(SynchronizerContext const& other) = delete;
    SynchronizerContext& operator=(SynchronizerContext const& other) = delete;

    [[nodiscard]] ExchangeRole GetExchangeRole() const;
    [[nodiscard]] std::weak_ptr<Configuration::Options::SupportedAlgorithms> const& GetSupportedAlgorithms() const;
    [[nodiscard]] std::optional<CipherSuite> const& GetCipherSuite() const;
    [[nodiscard]] std::unique_ptr<CipherPackage> const& GetCipherPackage() const;
    [[nodiscard]] std::size_t GetPublicKeySize() const;
    [[nodiscard]] std::size_t GetSaltSize() const;
    [[nodiscard]] std::size_t GetSignatureSize() const;
    [[nodiscard]] std::size_t GetVerificationDataSize() const;
    [[nodiscard]] bool IsKeyStoreAvailable() const;
    [[nodiscard]] bool IsCipherPackageReady() const;
    [[nodiscard]] std::unique_ptr<CipherPackage>&& ReleaseCipherPackage();

    [[nodiscard]] OptionalPublicKeyAndSalt GetPublicKeyAndSalt();

    [[nodiscard]] std::optional<CipherSuite> CreateMutualCipherSuite(
        std::string_view keyAgreement, std::string_view cipher, std::string_view hashFunction) const;

    [[nodiscard]] ConfidentialityLevel GetConfidentialityLevelOfCipherSuite(
        std::string_view keyAgreement, std::string_view cipher, std::string_view hashFunction) const;

    [[nodiscard]] Security::Salt const&  SetupKeyShare(
        std::optional<CipherSuite>&& optCipherSuite, PublicKey&& publicKey);

    [[nodiscard]] bool SetPeerPublicKeyAndSalt(PublicKey&& publicKey, Salt const& salt);

    // Note: It is no longer valid to use the key store after this method is called.
    [[nodiscard]] OptionalBuffer GenerateSessionKeys(SharedSecret&& sharedSecret);

    [[nodiscard]] VerificationStatus VerifyKeyShare(ReadableView verificationData) const;

private:
    ExchangeRole m_role;
    std::weak_ptr<Configuration::Options::SupportedAlgorithms> m_wpSupportedAlgorithms;
    std::optional<CipherSuite> m_optCipherSuite;
    std::optional<KeyStore> m_optKeyStore;
    Security::OptionalSecureBuffer m_optVerificationData;
    std::unique_ptr<CipherPackage> m_upCipherPackage;
};

//----------------------------------------------------------------------------------------------------------------------