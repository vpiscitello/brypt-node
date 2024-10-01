//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Security/Algorithms.hpp"
#include "Components/Security/CipherPackage.hpp"
#include "Components/Security/KeyStore.hpp"
#include "Components/Security/Classical/EllipticCurveDiffieHellmanModel.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] bool IsEllipticCurveKeyAgreement(std::string const& keyAgreement);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

static inline std::string const KeyAgreementName = "ecdh-p-384";
static inline std::string const CipherName = "aes-256-ctr";
static inline std::string const HashFunctionName = "sha384";

static constexpr std::size_t ExpectedPublicKeySize = 97;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, ModelSetupTest)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Classical::EllipticCurveDiffieHellmanModel model;

    EXPECT_FALSE(model.HasSupplementalData()); // The model should not indicate it injects supplemental data into key exchange requests. 
    EXPECT_EQ(model.GetSupplementalDataSize(), std::size_t{0}); // The model should not indicate it injects supplemental data into key exchange requests. 

    EXPECT_TRUE(model.IsKeyAgreementSupported(test::KeyAgreementName)); // The model should support a known key encapsulation algorithm name. 

    auto optPublicKey = model.SetupKeyExchange(cipherSuite);
    ASSERT_TRUE(optPublicKey);
    EXPECT_EQ(optPublicKey->GetSize(), test::ExpectedPublicKeySize); // The size of the public key should be equal to the expected size. 

    Security::KeyStore store{ std::move(*optPublicKey) };
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, ModelComputeSharedSecretWithEmptyPublicKey)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Classical::EllipticCurveDiffieHellmanModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));
    EXPECT_FALSE(model.ComputeSharedSecret(Security::PublicKey{})); // The model should fail to generate session keys using empty supplementary data.
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, ModelComputeSharedSecretWithSmallPublicKey)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Classical::EllipticCurveDiffieHellmanModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));

    Security::PublicKey publicKey{ Security::Test::GenerateGarbageData(test::ExpectedPublicKeySize - 1) };
    EXPECT_FALSE(model.ComputeSharedSecret(publicKey));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, ModelComputeSharedSecretWithLargePublicKey)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Classical::EllipticCurveDiffieHellmanModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));

    Security::PublicKey publicKey{ Security::Test::GenerateGarbageData(std::numeric_limits<std::uint16_t>::max()) };
    EXPECT_FALSE(model.ComputeSharedSecret(publicKey));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, ModelComputeSharedSecretWithEmptySupplementalData)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Classical::EllipticCurveDiffieHellmanModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));
    EXPECT_FALSE(model.ComputeSharedSecret(Security::SupplementalData{})); // The model should fail to generate session keys using empty supplementary data.
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, ModelComputeSharedSecretWithLargeSupplementalData)
{
    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Classical::EllipticCurveDiffieHellmanModel model;

    EXPECT_TRUE(model.SetupKeyExchange(cipherSuite));

    Security::SupplementalData supplementalData{ Security::Test::GenerateGarbageData(std::numeric_limits<std::uint16_t>::max()) };
    EXPECT_FALSE(model.ComputeSharedSecret(supplementalData));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, InvalidKeyAgreementTest)
{
    constexpr std::string_view TestInvalidKeyAgreementName = "ecdh-invalid-algorithm";

    Security::CipherSuite const cipherSuite{ Security::ConfidentialityLevel::High, TestInvalidKeyAgreementName, test::CipherName, test::HashFunctionName };
    Security::Classical::EllipticCurveDiffieHellmanModel model;

    EXPECT_FALSE(model.IsKeyAgreementSupported(std::string{ TestInvalidKeyAgreementName })); // The model should support a known key encapsulation algorithm name. 

    EXPECT_FALSE(model.SetupKeyExchange(cipherSuite)); // The model should fail to setup in the event an invalid key agreement scheme is used. 
    EXPECT_EQ(model.GetSupplementalDataSize(), std::size_t{0}); // The model should not indicate it injects supplemental data into key exchange requests. 
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, SynchronizationTest)
{
    for (auto const& keyAgreement : Security::SupportedKeyAgreementNames) {
        if (local::IsEllipticCurveKeyAgreement(keyAgreement)) {
            Security::CipherSuite const cipherSuite{
                Security::ConfidentialityLevel::High, keyAgreement, test::CipherName, test::HashFunctionName
            };
    
            Security::Classical::EllipticCurveDiffieHellmanModel initiatorModel;
            auto initiatorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = initiatorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Classical::EllipticCurveDiffieHellmanModel acceptorModel;
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

            auto const& [initiatorSharedSecret, initiatorSupplementalData] = *optInitiatorResult;
            EXPECT_FALSE(initiatorSharedSecret.IsEmpty());
            EXPECT_TRUE(initiatorSupplementalData.IsEmpty());

            auto const optAcceptorResult = acceptorModel.ComputeSharedSecret(*acceptorKeyStore.GetPeerPublicKey());
            ASSERT_TRUE(optInitiatorResult);

            auto const& [acceptorSharedSecret, acceptorSupplementalData] = *optAcceptorResult;
            EXPECT_FALSE(acceptorSharedSecret.IsEmpty());
            EXPECT_TRUE(acceptorSupplementalData.IsEmpty());

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

TEST(EllipticCurveSynchronizerSuite, SynchronizeWithGarabagePublicKeyTest)
{
    for (auto const& keyAgreement : Security::SupportedKeyAgreementNames) {
        if (local::IsEllipticCurveKeyAgreement(keyAgreement)) {
            Security::CipherSuite const cipherSuite{
                Security::ConfidentialityLevel::High, keyAgreement, test::CipherName, test::HashFunctionName
            };
    
            Security::Classical::EllipticCurveDiffieHellmanModel initiatorModel;
            auto initiatorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = initiatorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Classical::EllipticCurveDiffieHellmanModel acceptorModel;
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
            ASSERT_FALSE(optInitiatorResult); // Technically there is a non-zero chance the garbage data has the correct encoding and passes this. 
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, SynchronizeWithGarabageSupplementalDataTest)
{
    for (auto const& keyAgreement : Security::SupportedKeyAgreementNames) {
        if (local::IsEllipticCurveKeyAgreement(keyAgreement)) {
            Security::CipherSuite const cipherSuite{
                Security::ConfidentialityLevel::High, keyAgreement, test::CipherName, test::HashFunctionName
            };
    
            Security::Classical::EllipticCurveDiffieHellmanModel initiatorModel;
            auto initiatorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = initiatorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Classical::EllipticCurveDiffieHellmanModel acceptorModel;
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
            auto const supplementalData = Security::SupplementalData{ Security::Test::GenerateGarbageData(acceptorKeyStore.GetPublicKeySize()) };

            auto const optInitiatorResult = initiatorModel.ComputeSharedSecret(supplementalData);
            EXPECT_FALSE(optInitiatorResult);

            auto const optAcceptorResult = acceptorModel.ComputeSharedSecret(supplementalData);
            EXPECT_FALSE(optInitiatorResult);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, SynchronizeWithInjectedInitiatorPublicKeyTest)
{
    for (auto const& keyAgreement : Security::SupportedKeyAgreementNames) {
        if (local::IsEllipticCurveKeyAgreement(keyAgreement)) {
            Security::CipherSuite const cipherSuite{
                Security::ConfidentialityLevel::High, keyAgreement, test::CipherName, test::HashFunctionName
            };
    
            Security::Classical::EllipticCurveDiffieHellmanModel initiatorModel;
            auto initiatorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = initiatorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Classical::EllipticCurveDiffieHellmanModel acceptorModel;
            auto acceptorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = acceptorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Classical::EllipticCurveDiffieHellmanModel injectingModel;
            auto injectorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = injectingModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            auto const initiatorDefaultSalt = initiatorKeyStore.GetSalt();
            auto const injectorDefaultSalt = injectorKeyStore.GetSalt();
    
            {
                initiatorKeyStore.SetPeerPublicKey(Security::PublicKey{ injectorKeyStore.GetPublicKey() });
                initiatorKeyStore.PrependSessionSalt(injectorDefaultSalt);
            }

            {
                acceptorKeyStore.SetPeerPublicKey(Security::PublicKey{ initiatorKeyStore.GetPublicKey() });
                acceptorKeyStore.AppendSessionSalt(initiatorDefaultSalt);
            }

            auto const optInitiatorResult = initiatorModel.ComputeSharedSecret(*initiatorKeyStore.GetPeerPublicKey());
            ASSERT_TRUE(optInitiatorResult);

            auto const& [initiatorSharedSecret, initiatorSupplementalData] = *optInitiatorResult;
            EXPECT_FALSE(initiatorSharedSecret.IsEmpty());
            EXPECT_TRUE(initiatorSupplementalData.IsEmpty());

            auto const optAcceptorResult = acceptorModel.ComputeSharedSecret(*acceptorKeyStore.GetPeerPublicKey());
            ASSERT_TRUE(optInitiatorResult);

            auto const& [acceptorSharedSecret, acceptorSupplementalData] = *optAcceptorResult;
            EXPECT_FALSE(acceptorSharedSecret.IsEmpty());
            EXPECT_TRUE(acceptorSupplementalData.IsEmpty());

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

//----------------------------------------------------------------------------------------------------------------------

TEST(EllipticCurveSynchronizerSuite, SynchronizeWithInjectedAcceptorPublicKeyTest)
{
    for (auto const& keyAgreement : Security::SupportedKeyAgreementNames) {
        if (local::IsEllipticCurveKeyAgreement(keyAgreement)) {
            Security::CipherSuite const cipherSuite{ 
                Security::ConfidentialityLevel::High, keyAgreement, test::CipherName, test::HashFunctionName
            };
    
            Security::Classical::EllipticCurveDiffieHellmanModel initiatorModel;
            auto initiatorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = initiatorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Classical::EllipticCurveDiffieHellmanModel acceptorModel;
            auto acceptorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = acceptorModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            Security::Classical::EllipticCurveDiffieHellmanModel injectingModel;
            auto injectorKeyStore = [&] () -> Security::KeyStore {
                auto optPublicKey = injectingModel.SetupKeyExchange(cipherSuite);
                EXPECT_TRUE(optPublicKey);
                return Security::KeyStore{ std::move(*optPublicKey) };
            }();

            auto const acceptorDefaultSalt = acceptorKeyStore.GetSalt();
            auto const injectorDefaultSalt = injectorKeyStore.GetSalt();

            {
                initiatorKeyStore.SetPeerPublicKey(Security::PublicKey{ acceptorKeyStore.GetPublicKey() });
                initiatorKeyStore.PrependSessionSalt(acceptorDefaultSalt);
            }

            {
                acceptorKeyStore.SetPeerPublicKey(Security::PublicKey{ injectorKeyStore.GetPublicKey() });
                acceptorKeyStore.AppendSessionSalt(injectorDefaultSalt);
            }

            auto const optInitiatorResult = initiatorModel.ComputeSharedSecret(*initiatorKeyStore.GetPeerPublicKey());
            ASSERT_TRUE(optInitiatorResult);

            auto const& [initiatorSharedSecret, initiatorSupplementalData] = *optInitiatorResult;
            EXPECT_FALSE(initiatorSharedSecret.IsEmpty());
            EXPECT_TRUE(initiatorSupplementalData.IsEmpty());

            auto const optAcceptorResult = acceptorModel.ComputeSharedSecret(*acceptorKeyStore.GetPeerPublicKey());
            ASSERT_TRUE(optInitiatorResult);

            auto const& [acceptorSharedSecret, acceptorSupplementalData] = *optAcceptorResult;
            EXPECT_FALSE(acceptorSharedSecret.IsEmpty());
            EXPECT_TRUE(acceptorSupplementalData.IsEmpty());

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

            EXPECT_NE(initiatorKeyStore.GetPublicKey(), acceptorKeyStore.GetPeerPublicKey());
            EXPECT_EQ(initiatorKeyStore.GetPeerPublicKey(), acceptorKeyStore.GetPublicKey());
            EXPECT_NE(initiatorKeyStore.GetContentKey(), acceptorKeyStore.GetPeerContentKey());
            EXPECT_NE(initiatorKeyStore.GetPeerContentKey(), acceptorKeyStore.GetContentKey());
            EXPECT_NE(initiatorKeyStore.GetSignatureKey(), acceptorKeyStore.GetPeerSignatureKey());
            EXPECT_NE(initiatorKeyStore.GetPeerSignatureKey(), acceptorKeyStore.GetSignatureKey());
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool local::IsEllipticCurveKeyAgreement(std::string const& keyAgreement)
{
    return keyAgreement.starts_with("ecdh-");
}

//----------------------------------------------------------------------------------------------------------------------
