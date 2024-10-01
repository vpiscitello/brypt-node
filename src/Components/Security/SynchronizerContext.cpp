//---------------------------------------------------------------------------------------------------------------------
// File: SynchronizerContext.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "SynchronizerContext.hpp"
#include "CipherPackage.hpp"
#include "CipherService.hpp"
#include "Components/Core/ServiceProvider.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/crypto.h>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

Security::Detail::SynchronizerContext::SynchronizerContext(
    ExchangeRole role, std::weak_ptr<Configuration::Options::SupportedAlgorithms> const& wpSupportedAlgorithms)
    : m_role(role)
    , m_wpSupportedAlgorithms(wpSupportedAlgorithms)
    , m_optCipherSuite()
    , m_optKeyStore()
    , m_optVerificationData()
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::Detail::SynchronizerContext::SynchronizerContext(SynchronizerContext&& other) noexcept
    : m_role(std::move(other.m_role))
    , m_wpSupportedAlgorithms(std::move(other.m_wpSupportedAlgorithms))
    , m_optCipherSuite(std::move(other.m_optCipherSuite))
    , m_optKeyStore(std::move(other.m_optKeyStore))
    , m_optVerificationData(std::move(other.m_optVerificationData))
{

}

//----------------------------------------------------------------------------------------------------------------------
Security::Detail::SynchronizerContext& Security::Detail::SynchronizerContext::operator=(SynchronizerContext&& other) noexcept
{
    m_role = std::move(other.m_role);
    m_wpSupportedAlgorithms = std::move(other.m_wpSupportedAlgorithms);
    m_optCipherSuite = std::move(other.m_optCipherSuite);
    m_optKeyStore = std::move(other.m_optKeyStore);
    m_optVerificationData = std::move(other.m_optVerificationData);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Security::ExchangeRole Security::Detail::SynchronizerContext::GetExchangeRole() const { return m_role; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Configuration::Options::SupportedAlgorithms> const& Security::Detail::SynchronizerContext::GetSupportedAlgorithms() const
{
    return m_wpSupportedAlgorithms;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Security::CipherSuite> const& Security::Detail::SynchronizerContext::GetCipherSuite() const
{
    return m_optCipherSuite;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Detail::SynchronizerContext::GetPublicKeySize() const
{
    return (m_optKeyStore) ? m_optKeyStore->GetPublicKeySize() : std::size_t{ 0 };
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Detail::SynchronizerContext::GetSaltSize() const { return KeyStore::PrincipalRandomSize; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Detail::SynchronizerContext::GetSignatureSize() const
{
    return (m_optCipherSuite) ? m_optCipherSuite->GetSignatureSize() : std::size_t{ 0 };
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Detail::SynchronizerContext::GetVerificationDataSize() const
{
   
    return (m_optCipherSuite && m_optVerificationData) ?
        m_optCipherSuite->GetEncryptedSize(m_optVerificationData->GetSize()) : std::size_t{ 0 };
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Detail::SynchronizerContext::IsKeyStoreAvailable() const { return m_optKeyStore.has_value(); }

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Security::CipherPackage> const& Security::Detail::SynchronizerContext::GetCipherPackage() const
{
    return m_upCipherPackage;
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Detail::SynchronizerContext::IsCipherPackageReady() const { return m_upCipherPackage != nullptr; }


//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Security::CipherPackage>&& Security::Detail::SynchronizerContext::ReleaseCipherPackage()
{
    return std::move(m_upCipherPackage);
}

//----------------------------------------------------------------------------------------------------------------------

Security::Detail::SynchronizerContext::OptionalPublicKeyAndSalt Security::Detail::SynchronizerContext::GetPublicKeyAndSalt()
{
    if (!m_optKeyStore) {
        return {}; // The synchronizer should still have ownership of the key store.  
    }

    return std::make_pair(m_optKeyStore->GetPublicKey().GetData(), m_optKeyStore->GetSalt().GetData());
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Security::CipherSuite> Security::Detail::SynchronizerContext::CreateMutualCipherSuite(
    std::string_view keyAgreement, std::string_view cipher, std::string_view hashFunction) const
{
    auto const level = GetConfidentialityLevelOfCipherSuite(keyAgreement, cipher, hashFunction);
   
    // If a confidentiality level could not be determined using the provided algorithm names, an error has occurred. 
    // This indicates that the peer may be negotiating in bad faith given no valid cipher package can be created. 
    if (level == ConfidentialityLevel::Unknown) {
        return {};  // Return early given an error has occurred. 
    }

    std::optional<CipherSuite> optCipherSuite;
    try {
        optCipherSuite = CipherSuite{ level, keyAgreement, cipher, hashFunction };
    } catch (...) {
        return {};
    }

    return optCipherSuite;
}

//----------------------------------------------------------------------------------------------------------------------

Security::ConfidentialityLevel Security::Detail::SynchronizerContext::GetConfidentialityLevelOfCipherSuite(
    std::string_view keyAgreement, std::string_view cipher, std::string_view hashFunction) const
{
    using enum ConfidentialityLevel;

    ConfidentialityLevel keyAgreementLevel = Unknown;
    ConfidentialityLevel cipherLevel = Unknown;
    ConfidentialityLevel hasFunctionLevel = Unknown;

    auto const spSupportedAlgorithms = m_wpSupportedAlgorithms.lock();
    if (!spSupportedAlgorithms) { return Unknown; }

    // Iterate over the supported algorithms until the highest level the key agreement, cipher, and hash function is 
    // associated with. We look for the highest because the user has indicated it may be used at higher level vs. it 
    // being strictly lower tier of method. 
    spSupportedAlgorithms->ForEachSupportedAlgorithm([&] (ConfidentialityLevel level, Configuration::Options::Algorithms const& algorithms) {
        if (keyAgreementLevel < level && std::ranges::contains(algorithms.GetKeyAgreements(), keyAgreement)) {
            keyAgreementLevel = level;
        }

        if (cipherLevel < level && std::ranges::contains(algorithms.GetCiphers(), cipher)) {
            cipherLevel = level;
        }

        if (hasFunctionLevel < level && std::ranges::contains(algorithms.GetHashFunctions(), hashFunction)) {
            hasFunctionLevel = level;
        }

        bool const foundHighestConfidentialityLevels = 
            keyAgreementLevel != Unknown && cipherLevel != Unknown && hasFunctionLevel != Unknown;
        if (foundHighestConfidentialityLevels) {
            return CallbackIteration::Stop; // Stop searching as soon as the highest level for each component is found. 
        }

        return CallbackIteration::Continue;
    });

    // Determine the lowest confidentiality that this cipher suite may be used for. Only one component needs to
    // be a lower level to degrade the entire suite. 
    ConfidentialityLevel lowestLevel = keyAgreementLevel;
    if (cipherLevel < lowestLevel) { lowestLevel = cipherLevel; }
    if (hasFunctionLevel < lowestLevel) { lowestLevel = hasFunctionLevel; }

    return lowestLevel;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Salt const& Security::Detail::SynchronizerContext::SetupKeyShare(
    std::optional<CipherSuite>&& optCipherSuite, PublicKey&& publicKey)
{
    assert(optCipherSuite); // The type is only optional, because we move a return result after processing. 
    m_optCipherSuite = std::move(optCipherSuite);
    m_optKeyStore = KeyStore{ std::move(publicKey) };
    return m_optKeyStore->GetSalt();
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Detail::SynchronizerContext::SetPeerPublicKeyAndSalt(PublicKey&& publicKey, Salt const& salt)
{
    if (!m_optKeyStore) {
        return false; // This operation is not valid if the key store is not accessible. 
    }

    m_optKeyStore->SetPeerPublicKey(std::move(publicKey));

    switch (m_role) {
        case Security::ExchangeRole::Initiator: m_optKeyStore->PrependSessionSalt(salt); break;
        case Security::ExchangeRole::Acceptor: m_optKeyStore->AppendSessionSalt(salt); break;
        default: return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Security::OptionalBuffer Security::Detail::SynchronizerContext::GenerateSessionKeys(SharedSecret&& sharedSecret)
{
    if (!m_optCipherSuite || !m_optKeyStore) {
        return {}; // We are in an error state if we do not have a cipher suite or the key store. 
    }

    m_optVerificationData = m_optKeyStore->GenerateSessionKeys(m_role, *m_optCipherSuite, std::move(sharedSecret));
    if (!m_optVerificationData) {
        return {}; // If the key store fails to generate session keys using the shared secret, an error has occurred. 
    }

    // After the key store has successfully generated session keys, a cipher package may now be created. The cipher
    // package will take ownsership of the synchronizer's key store. It is no longer valid to use the key store within
    // the synchronizer or it's context after this point. 
    m_upCipherPackage = std::make_unique<CipherPackage>(*m_optCipherSuite, std::move(*m_optKeyStore));
    m_optKeyStore = std::nullopt; // Explicitly set the moved key store object to nullopt. 

    // Generate the verification data needed for the response. If the process fails, return an error. 
    return m_upCipherPackage->Encrypt(m_optVerificationData->GetData());  // Encrypt data to challenge the peer's keys. 
}

//----------------------------------------------------------------------------------------------------------------------

Security::VerificationStatus Security::Detail::SynchronizerContext::VerifyKeyShare(ReadableView verificationData) const
{
    if (!m_upCipherPackage || !m_optVerificationData) { 
        return VerificationStatus::Failed;
    }

    // Decrypt the provided data to get the peer's verification data. 
    auto const optDecryptedData = m_upCipherPackage->Decrypt(verificationData);
    if (!optDecryptedData) {
        return VerificationStatus::Failed;
    }

    // Verify the provided verification data matches the verification data we have generated. 
    // If the data does not match, it is an error. 
	auto const result = CRYPTO_memcmp(
		optDecryptedData->data(), m_optVerificationData->GetData().data(), m_optVerificationData->GetSize());

	return (result == 0) ? VerificationStatus::Success : VerificationStatus::Failed;
}

//----------------------------------------------------------------------------------------------------------------------
