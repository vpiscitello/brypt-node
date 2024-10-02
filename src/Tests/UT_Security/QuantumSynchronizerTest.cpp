//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Security/Algorithms.hpp"
#include "Components/Security/CipherPackage.hpp"
#include "Components/Security/KeyStore.hpp"
#include "Components/Security/PostQuantum/KeyEncapsulationModel.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] bool IsQuantumKeyAgreement(std::string const& keyAgreement);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

static inline std::string const KeyAgreementName = "kem-kyber768";
static inline std::string const CipherName = "aes-256-ctr";
static inline std::string const HashFunctionName = "sha384";

static constexpr std::size_t ExpectedPublicKeySize = 1184;
static constexpr std::size_t ExpectedEncapsulationSize = 1088 ;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, ModelSetupTest)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Quantum::KeyEncapsulationModel model;

    EXPECT_TRUE(model.HasSupplementalData()); // The model should indicate it injects supplemental data into key exchange requests. 
    EXPECT_ANY_THROW(model.GetSupplementalDataSize()); // The model should throw is operations requiring setup are called before setup has occurred. 

    EXPECT_TRUE(model.IsKeyAgreementSupported(test::KeyAgreementName)); // The model should support a known key encapsulation algorithm name. 

    auto optPublicKey = model.SetupKeyExchange(cipherSuite);
    ASSERT_TRUE(optPublicKey);
    EXPECT_EQ(optPublicKey->GetSize(), test::ExpectedPublicKeySize); // The size of the public key should be equal to the expected size. 

    Security::KeyStore store{ std::move(*optPublicKey) };

    EXPECT_EQ(model.GetSupplementalDataSize(), test::ExpectedEncapsulationSize); // After the model has been setup, the supplemental data size should be equal to the expected value. 
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, ModelComputeSharedSecretWithEmptyPublicKey)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Quantum::KeyEncapsulationModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));
    EXPECT_FALSE(model.ComputeSharedSecret(Security::PublicKey{})); // The model should fail to generate session keys using empty supplementary data.
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, ModelComputeSharedSecretWithSmallPublicKey)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Quantum::KeyEncapsulationModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));

    Security::PublicKey publicKey{ Security::Test::GenerateGarbageData(test::ExpectedPublicKeySize - 1) };
    EXPECT_FALSE(model.ComputeSharedSecret(publicKey));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, ModelComputeSharedSecretWithLargePublicKey)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Quantum::KeyEncapsulationModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));

    Security::PublicKey publicKey{ Security::Test::GenerateGarbageData(std::numeric_limits<std::uint16_t>::max()) };
    EXPECT_FALSE(model.ComputeSharedSecret(publicKey));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, ModelComputeSharedSecretWithEmptySupplementalData)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Quantum::KeyEncapsulationModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));
    EXPECT_FALSE(model.ComputeSharedSecret(Security::SupplementalData{})); // The model should fail to generate session keys using empty supplementary data.
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, ModelComputeSharedSecretWithSmallSupplementalData)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Quantum::KeyEncapsulationModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));

    Security::SupplementalData supplementalData{ Security::Test::GenerateGarbageData(model.GetSupplementalDataSize() - 1) };
    EXPECT_FALSE(model.ComputeSharedSecret(supplementalData));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, ModelComputeSharedSecretWithLargeSupplementalData)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Quantum::KeyEncapsulationModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));

    Security::SupplementalData supplementalData{ Security::Test::GenerateGarbageData(std::numeric_limits<std::uint16_t>::max()) };
    EXPECT_FALSE(model.ComputeSharedSecret(supplementalData));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, InvalidKeyAgreementTest)
{
    constexpr std::string_view TestInvalidKeyAgreementName = "kem-invalid-algorithm";

    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, TestInvalidKeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Quantum::KeyEncapsulationModel model;

    EXPECT_FALSE(model.IsKeyAgreementSupported(std::string{ TestInvalidKeyAgreementName })); // The model should support a known key encapsulation algorithm name. 

    EXPECT_FALSE(model.SetupKeyExchange(cipherSuite)); // The model should fail to setup in the event an invalid key agreement scheme is used. 
    EXPECT_ANY_THROW(model.GetSupplementalDataSize()); // The model should throw is operations requiring setup are called before setup has occurred. 
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, SynchronizationTest)
{
    for (auto const& keyAgreement : Security::SupportedKeyAgreementNames) {
        if (local::IsQuantumKeyAgreement(keyAgreement)) {
            Security::CipherSuite const cipherSuite{
                Security::ConfidentialityLevel::High, keyAgreement, test::CipherName, test::HashFunctionName
            };
    
            Security::Quantum::KeyEncapsulationModel initiatorModel;
            auto initiatorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = initiatorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Quantum::KeyEncapsulationModel acceptorModel;
            auto acceptorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = acceptorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            auto const initiatorDefaultSalt = initiatorKeyStore.GetSalt();
            auto const acceptorDefaultSalt = acceptorKeyStore.GetSalt();
    
            {
                initiatorKeyStore.SetPeerPublicKey(Security::PublicKey{ acceptorKeyStore.GetPublicKey() });
                initiatorKeyStore.PrependSessionSalt(acceptorDefaultSalt);
            }

            {
                acceptorKeyStore.SetPeerPublicKey(Security::PublicKey{ initiatorKeyStore.GetPublicKey() });
                acceptorKeyStore.AppendSessionSalt(initiatorDefaultSalt);
            }

            auto const optInitiatorResult = initiatorModel.ComputeSharedSecret(*initiatorKeyStore.GetPeerPublicKey());
            ASSERT_TRUE(optInitiatorResult);

            auto const& [initiatorSharedSecret, supplementalData] = *optInitiatorResult;
            EXPECT_FALSE(initiatorSharedSecret.IsEmpty());
            EXPECT_EQ(supplementalData.GetSize(), initiatorModel.GetSupplementalDataSize());

            auto const optAcceptorResult = acceptorModel.ComputeSharedSecret(supplementalData);
            ASSERT_TRUE(optAcceptorResult);

            auto const& acceptorSharedSecret = *optAcceptorResult;
            EXPECT_FALSE(acceptorSharedSecret.IsEmpty());

            EXPECT_EQ(initiatorSharedSecret, acceptorSharedSecret);

            auto const optInitiatorVerificationData = initiatorKeyStore.GenerateSessionKeys(
                Security::ExchangeRole::Initiator, cipherSuite, initiatorSharedSecret);
            ASSERT_TRUE(optInitiatorVerificationData);
            EXPECT_TRUE(initiatorKeyStore.HasGeneratedKeys());

            auto const optAcceptorVerificationData = acceptorKeyStore.GenerateSessionKeys(
                Security::ExchangeRole::Acceptor, cipherSuite, acceptorSharedSecret);
            ASSERT_TRUE(optAcceptorVerificationData);
            EXPECT_TRUE(acceptorKeyStore.HasGeneratedKeys());

            EXPECT_EQ(*optInitiatorVerificationData, *optAcceptorVerificationData);

            EXPECT_EQ(initiatorKeyStore.GetPublicKey(), acceptorKeyStore.GetPeerPublicKey());
            EXPECT_EQ(initiatorKeyStore.GetPeerPublicKey(), acceptorKeyStore.GetPublicKey());
            EXPECT_EQ(initiatorKeyStore.GetContentKey(), acceptorKeyStore.GetPeerContentKey());
            EXPECT_EQ(initiatorKeyStore.GetPeerContentKey(), acceptorKeyStore.GetContentKey());
            EXPECT_EQ(initiatorKeyStore.GetSignatureKey(), acceptorKeyStore.GetPeerSignatureKey());
            EXPECT_EQ(initiatorKeyStore.GetPeerSignatureKey(), acceptorKeyStore.GetSignatureKey());
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, SynchronizeWithMutatedPublicKeyTest)
{
    for (auto const& keyAgreement : Security::SupportedKeyAgreementNames) {
        if (local::IsQuantumKeyAgreement(keyAgreement)) {
            Security::CipherSuite const cipherSuite{
                Security::ConfidentialityLevel::High, keyAgreement, test::CipherName, test::HashFunctionName
            };
    
            Security::Quantum::KeyEncapsulationModel initiatorModel;
            auto initiatorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = initiatorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Quantum::KeyEncapsulationModel acceptorModel;
            auto acceptorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = acceptorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            auto const initiatorDefaultSalt = initiatorKeyStore.GetSalt();
            auto const acceptorDefaultSalt = acceptorKeyStore.GetSalt();
    
            {
                auto mutated = Security::PublicKey{ Security::Test::GenerateGarbageData(acceptorKeyStore.GetPublicKeySize()) };
                while (mutated == acceptorKeyStore.GetPublicKey()) {
                    mutated = Security::PublicKey{ Security::Test::GenerateGarbageData(acceptorKeyStore.GetPublicKeySize()) };
                }
                initiatorKeyStore.SetPeerPublicKey(std::move(mutated));
                initiatorKeyStore.PrependSessionSalt(acceptorDefaultSalt);
            }

            {
                acceptorKeyStore.SetPeerPublicKey(Security::PublicKey{ initiatorKeyStore.GetPublicKey() });
                acceptorKeyStore.AppendSessionSalt(initiatorDefaultSalt);
            }

            auto const optInitiatorResult = initiatorModel.ComputeSharedSecret(*initiatorKeyStore.GetPeerPublicKey());

            // The mutated key may or may not result in a valid shared secret. If it does, validate that the resulting shared
            // secret can not be used to finalize the syncronization process. 
            if (optInitiatorResult) {
                auto const& [initiatorSharedSecret, supplementalData] = *optInitiatorResult;
                EXPECT_FALSE(initiatorSharedSecret.IsEmpty());
                EXPECT_EQ(supplementalData.GetSize(), initiatorModel.GetSupplementalDataSize());

                // There are two possible results from computing a shared secret using malformed data, either the external library 
                // will detect the mutation and throw an error or not. In the case it is not detected, we need to verify that the
                // two sides do not have a secret that actually matches. 
                if (auto const optAcceptorResult = acceptorModel.ComputeSharedSecret(supplementalData); optAcceptorResult) {
                    auto const& acceptorSharedSecret = *optAcceptorResult;
                    EXPECT_FALSE(acceptorSharedSecret.IsEmpty());

                    // If the model uses a public key that has been altered, the resulting shared secret should not be the same. 
                    EXPECT_NE(initiatorSharedSecret, acceptorSharedSecret);

                    auto const optInitiatorVerificationData = initiatorKeyStore.GenerateSessionKeys(
                        Security::ExchangeRole::Initiator, cipherSuite, initiatorSharedSecret);
                    ASSERT_TRUE(optInitiatorVerificationData);
                    EXPECT_TRUE(initiatorKeyStore.HasGeneratedKeys());

                    auto const optAcceptorVerificationData = acceptorKeyStore.GenerateSessionKeys(
                        Security::ExchangeRole::Acceptor, cipherSuite, acceptorSharedSecret);
                    ASSERT_TRUE(optAcceptorVerificationData);
                    EXPECT_TRUE(acceptorKeyStore.HasGeneratedKeys());

                    // Using a shared secret that differs should result in keys that don't match. 
                    EXPECT_NE(*optInitiatorVerificationData, *optAcceptorVerificationData);

                    EXPECT_EQ(initiatorKeyStore.GetPublicKey(), acceptorKeyStore.GetPeerPublicKey());
                    EXPECT_NE(initiatorKeyStore.GetPeerPublicKey(), acceptorKeyStore.GetPublicKey());
                    EXPECT_NE(initiatorKeyStore.GetContentKey(), acceptorKeyStore.GetPeerContentKey());
                    EXPECT_NE(initiatorKeyStore.GetPeerContentKey(), acceptorKeyStore.GetContentKey());
                    EXPECT_NE(initiatorKeyStore.GetSignatureKey(), acceptorKeyStore.GetPeerSignatureKey());
                    EXPECT_NE(initiatorKeyStore.GetPeerSignatureKey(), acceptorKeyStore.GetSignatureKey());
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(QuantumSynchronizerSuite, SynchronizeWithMutatedSupplementalDataTest)
{
    for (auto const& keyAgreement : Security::SupportedKeyAgreementNames) {
        if (local::IsQuantumKeyAgreement(keyAgreement)) {
            Security::CipherSuite const cipherSuite{
                Security::ConfidentialityLevel::High, keyAgreement, test::CipherName, test::HashFunctionName
            };

            Security::Quantum::KeyEncapsulationModel initiatorModel;
            auto initiatorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = initiatorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Quantum::KeyEncapsulationModel acceptorModel;
            auto acceptorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = acceptorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            auto const initiatorDefaultSalt = initiatorKeyStore.GetSalt();
            auto const acceptorDefaultSalt = acceptorKeyStore.GetSalt();

            {
                initiatorKeyStore.SetPeerPublicKey(Security::PublicKey{ acceptorKeyStore.GetPublicKey() });
                initiatorKeyStore.PrependSessionSalt(acceptorDefaultSalt);
            }

            {
                acceptorKeyStore.SetPeerPublicKey(Security::PublicKey{ initiatorKeyStore.GetPublicKey() });
                acceptorKeyStore.AppendSessionSalt(initiatorDefaultSalt);
            }

            auto const optInitiatorResult = initiatorModel.ComputeSharedSecret(*initiatorKeyStore.GetPeerPublicKey());
            ASSERT_TRUE(optInitiatorResult);

            auto const& [initiatorSharedSecret, supplementalData] = *optInitiatorResult;
            EXPECT_FALSE(initiatorSharedSecret.IsEmpty());
            EXPECT_EQ(supplementalData.GetSize(), initiatorModel.GetSupplementalDataSize());

            auto mutated = Security::SupplementalData{ Security::Test::GenerateGarbageData(supplementalData.GetSize()) };
            while (mutated == supplementalData) {
                mutated = Security::SupplementalData{ Security::Test::GenerateGarbageData(supplementalData.GetSize()) };
            }

            // There are two possible results from computing a shared secret using malformed data, either the external library 
            // will detect the mutation and throw an error or not. In the case it is not detected, we need to verify that the
            // two sides do not have a secret that actually matches. 
            if (auto const optAcceptorResult = acceptorModel.ComputeSharedSecret(mutated); optAcceptorResult) {
                auto const& acceptorSharedSecret = *optAcceptorResult;
                EXPECT_FALSE(acceptorSharedSecret.IsEmpty());

                // If the model uses a public key that has been altered, the resulting shared secret should not be the same. 
                EXPECT_NE(initiatorSharedSecret, acceptorSharedSecret);

                auto const optInitiatorVerificationData = initiatorKeyStore.GenerateSessionKeys(
                    Security::ExchangeRole::Initiator, cipherSuite, initiatorSharedSecret);
                ASSERT_TRUE(optInitiatorVerificationData);
                EXPECT_TRUE(initiatorKeyStore.HasGeneratedKeys());

                auto const optAcceptorVerificationData = acceptorKeyStore.GenerateSessionKeys(
                    Security::ExchangeRole::Acceptor, cipherSuite, acceptorSharedSecret);
                ASSERT_TRUE(optAcceptorVerificationData);
                EXPECT_TRUE(acceptorKeyStore.HasGeneratedKeys());

                // Using a shared secret that differs should result in keys that don't match. 
                EXPECT_NE(*optInitiatorVerificationData, *optAcceptorVerificationData);

                EXPECT_EQ(initiatorKeyStore.GetPublicKey(), acceptorKeyStore.GetPeerPublicKey());
                EXPECT_EQ(initiatorKeyStore.GetPeerPublicKey(), acceptorKeyStore.GetPublicKey());
                EXPECT_NE(initiatorKeyStore.GetContentKey(), acceptorKeyStore.GetPeerContentKey());
                EXPECT_NE(initiatorKeyStore.GetPeerContentKey(), acceptorKeyStore.GetContentKey());
                EXPECT_NE(initiatorKeyStore.GetSignatureKey(), acceptorKeyStore.GetPeerSignatureKey());
                EXPECT_NE(initiatorKeyStore.GetPeerSignatureKey(), acceptorKeyStore.GetSignatureKey());
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool local::IsQuantumKeyAgreement(std::string const& keyAgreement)
{
    return keyAgreement.starts_with("kem-");
}

//----------------------------------------------------------------------------------------------------------------------
