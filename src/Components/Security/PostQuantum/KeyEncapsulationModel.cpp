//----------------------------------------------------------------------------------------------------------------------
// File: KeyEncapsulationModel.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "KeyEncapsulationModel.hpp"
#include "Components/Security/CipherPackage.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Message/PackUtils.hpp"
#include "Components/Security/SecurityUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

static std::unordered_map<std::string, std::string> const KeyEncapsulationMechanismMappings {
    { "kem-bike-l1", OQS_KEM_alg_bike_l1 },
    { "kem-bike-l3", OQS_KEM_alg_bike_l3 },
    { "kem-bike-l5", OQS_KEM_alg_bike_l5 },
    { "kem-classic-mceliece-348864", OQS_KEM_alg_classic_mceliece_348864 },
    { "kem-classic-mceliece-348864f", OQS_KEM_alg_classic_mceliece_348864f },
    { "kem-classic-mceliece-460896", OQS_KEM_alg_classic_mceliece_460896 },
    { "kem-classic-mceliece-460896f", OQS_KEM_alg_classic_mceliece_460896f },
    { "kem-classic-mceliece-6688128", OQS_KEM_alg_classic_mceliece_6688128 },
    { "kem-classic-mceliece-6688128f", OQS_KEM_alg_classic_mceliece_6688128f },
    { "kem-classic-mceliece-6960119", OQS_KEM_alg_classic_mceliece_6960119 },
    { "kem-classic-mceliece-6960119f", OQS_KEM_alg_classic_mceliece_6960119f },
    { "kem-classic-mceliece-8192128", OQS_KEM_alg_classic_mceliece_8192128 },
    { "kem-classic-mceliece-8192128f", OQS_KEM_alg_classic_mceliece_8192128f },
    { "kem-hqc-128", OQS_KEM_alg_hqc_128 },
    { "kem-hqc-192", OQS_KEM_alg_hqc_192 },
    { "kem-hqc-256", OQS_KEM_alg_hqc_256 },
    { "kem-kyber512", OQS_KEM_alg_kyber_512 },
    { "kem-kyber768", OQS_KEM_alg_kyber_768 },
    { "kem-kyber1024", OQS_KEM_alg_kyber_1024 },
    { "kem-sntruprime-sntrup761", OQS_KEM_alg_ntruprime_sntrup761 },
    { "kem-frodokem-640-aes", OQS_KEM_alg_frodokem_640_aes },
    { "kem-frodokem-640-shake", OQS_KEM_alg_frodokem_640_shake },
    { "kem-frodokem-976-aes", OQS_KEM_alg_frodokem_976_aes },
    { "kem-frodokem-976-shake", OQS_KEM_alg_frodokem_976_shake },
    { "kem-frodokem-1344-aes", OQS_KEM_alg_frodokem_1344_aes },
    { "kem-frodokem-1344-shake", OQS_KEM_alg_frodokem_1344_shake }
};

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Security::Quantum::KeyEncapsulationModel::KeyEncapsulator {
//----------------------------------------------------------------------------------------------------------------------

Security::Quantum::KeyEncapsulationModel::KeyEncapsulator::KeyEncapsulator(oqs::KeyEncapsulation&& kem)
    : m_kem(std::move(kem))
{
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Quantum::KeyEncapsulationModel::KeyEncapsulator::GetPublicKeySize() const
{
    return m_kem.get_details().length_public_key; 
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Quantum::KeyEncapsulationModel::KeyEncapsulator::GetEncapsulatedSize() const
{
    return m_kem.get_details().length_ciphertext; 
}

//----------------------------------------------------------------------------------------------------------------------

Security::Quantum::KeyEncapsulationModel::ComputeFromPublicKeyResult Security::Quantum::KeyEncapsulationModel::KeyEncapsulator::GenerateEncapsulatedSecret(
    PublicKey const& publicKey) const
{
    // Create an encapsulated shared secret using the peer's public key. If the process fails, synchronization failed and we cannot proceed. 
    try {
        if (publicKey.GetSize() != GetPublicKeySize()) {
            return {}; // If the peer's public key is empty, a shared secret cannot be computed. 
        }

        return publicKey.Read<ComputeFromPublicKeyResult>([&] (Buffer const& publicKey) {
            auto&& [encapsulated, secret] = m_kem.encap_secret(publicKey);
            return std::make_pair(SharedSecret{ std::move(secret) }, SupplementalData{ std::move(encapsulated) });
        });
    } catch(...) {
        return {};
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------

Security::Quantum::KeyEncapsulationModel::ComputeFromSupplementalDataResult Security::Quantum::KeyEncapsulationModel::KeyEncapsulator::DecapsulateSecret(
    SupplementalData const& supplementalData) const
{
    // Try to generate and decapsulate the shared secret. If the OQS method throws an error, signal that the operation did not succeed. 
    try {
        if (supplementalData.GetSize() != GetEncapsulatedSize()) {
            return {}; // If the size of the supplemental data is not equal to the expected encapsulated size, an error has occurred. 
        }

        return supplementalData.Read<ComputeFromSupplementalDataResult>([&] (Buffer const& data) {
            return SharedSecret{ m_kem.decap_secret(data) };
        });
    } catch(...) {
        return {};
    }

    return {};
}

//----------------------------------------------------------------------------------------------------------------------
// } Security::Quantum::KeyEncapsulationModel::Context 
//----------------------------------------------------------------------------------------------------------------------

Security::Quantum::KeyEncapsulationModel::KeyEncapsulationModel()
    : ISynchronizerModel()
    , m_upSessionEncapsulator()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Quantum::KeyEncapsulationModel::IsKeyAgreementSupported(std::string const& keyAgreement)
{
    return local::KeyEncapsulationMechanismMappings.contains(keyAgreement);
}

//----------------------------------------------------------------------------------------------------------------------

Security::Quantum::KeyEncapsulationModel::SetupResult Security::Quantum::KeyEncapsulationModel::SetupKeyExchange(
    CipherSuite const& cipherSuite)
{
    try {
        auto const& keyAgreement = cipherSuite.GetKeyAgreementName();
        if (auto const itr = local::KeyEncapsulationMechanismMappings.find(keyAgreement); itr != local::KeyEncapsulationMechanismMappings.end()) {
            auto const& [internal, external] = *itr;

            // Create the key encapsulation object using OQS. 
            oqs::KeyEncapsulation kem{ external.data() };

            // Generate the key pair for the key exchange. Capture the result as a typed public key. 
            PublicKey publicKey = { kem.generate_keypair() };
            if (publicKey.IsEmpty()) {
                return {}; // If the generated public key is empty, an error has occurred. 
            }

            // Create an encapsulator that may be used for key exchange. 
            m_upSessionEncapsulator = std::make_unique<KeyEncapsulator>(std::move(kem));

            return publicKey;
        }

    } catch (...) {
        return {}; // If we have failed to create a key encapsulator for the session, an error has occurred. 
    }

    return {}; // If have failed to setup the model state, an error has occurred. 
}


//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Generate and encapsulates an ephemeral session key using configured key encapsulation mechanism. The
// caller is provided the encapsulated shared secret to provide the peer. If an error is 
// encountered nullopt is provided instead. 
//----------------------------------------------------------------------------------------------------------------------
Security::Quantum::KeyEncapsulationModel::ComputeFromPublicKeyResult Security::Quantum::KeyEncapsulationModel::ComputeSharedSecret(
    PublicKey const& publicKey)
{
    if (!m_upSessionEncapsulator) { return {}; } // If we have not been setup yet it is an error. 

    // Use the session context to generate a secret for using the peer's public key. 
    return m_upSessionEncapsulator->GenerateEncapsulatedSecret(publicKey);
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Decapsulates an ephemeral session key using the key encapsulation mechanism from the provided
// encapsulated ciphertext. 
//----------------------------------------------------------------------------------------------------------------------
Security::Quantum::KeyEncapsulationModel::ComputeFromSupplementalDataResult Security::Quantum::KeyEncapsulationModel::ComputeSharedSecret(
    SupplementalData const& supplementalData)
{
    if (!m_upSessionEncapsulator) { return {}; }
    return m_upSessionEncapsulator->DecapsulateSecret(supplementalData);
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Quantum::KeyEncapsulationModel::HasSupplementalData() const { return true; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Quantum::KeyEncapsulationModel::GetSupplementalDataSize() const
{
    // We should always be calling this method after key exchange has been initialized. 
    if (!m_upSessionEncapsulator) {
        throw std::runtime_error("Unable to obtain key exchange details before initialization.");
    }

    return m_upSessionEncapsulator->GetEncapsulatedSize();
}

//----------------------------------------------------------------------------------------------------------------------
