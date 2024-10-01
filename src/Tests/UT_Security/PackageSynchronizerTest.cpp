//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Configuration/Options.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Message/MessageContext.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Security/Algorithms.hpp"
#include "Components/Security/CipherPackage.hpp"
#include "Components/Security/CipherService.hpp"
#include "Components/Security/PackageSynchronizer.hpp"
#include "Components/Security/KeyStore.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

std::string const KeyAgreementName = "kem-kyber768";
std::string const CipherName = "aes-256-ctr";
std::string const HashFunctionName = "sha384";

constexpr std::size_t ExpectedPublicKeySize = 1184;
constexpr std::size_t ExpectedEncapsulationSize = 1088;

Security::Buffer const Data{
    0x54, 0x6F, 0x20, 0x62, 0x65, 0x20, 0x6C, 0x6F,
    0x73, 0x74, 0x20, 0x69, 0x6E, 0x20, 0x74, 0x68,
    0x65, 0x20, 0x66, 0x6F, 0x72, 0x65, 0x73, 0x74,
    0x0A, 0x54, 0x6F, 0x20, 0x62, 0x65, 0x20, 0x63,
    0x75, 0x74, 0x20, 0x61, 0x64, 0x72, 0x69, 0x66,
    0x74, 0x0A, 0x59, 0x6F, 0x75, 0x27, 0x76, 0x65,
    0x20, 0x62, 0x65, 0x65, 0x6E, 0x20, 0x74, 0x72,
    0x79, 0x69, 0x6E, 0x67, 0x20, 0x74, 0x6F, 0x20,
    0x72, 0x65, 0x61, 0x63, 0x68, 0x20, 0x6D, 0x65,
    0x0A, 0x59, 0x6F, 0x75, 0x20, 0x62, 0x6F, 0x75,
    0x67, 0x68, 0x74, 0x20, 0x6D, 0x65, 0x20, 0x61,
    0x20, 0x62, 0x6F, 0x6F, 0x6B, 0x0A, 0x54, 0x6F,
    0x20, 0x62, 0x65, 0x20, 0x6C, 0x6F, 0x73, 0x74,
    0x20, 0x69, 0x6E, 0x20, 0x74, 0x68, 0x65, 0x20,
    0x66, 0x6F, 0x72, 0x65, 0x73, 0x74, 0x0A, 0x54,
    0x6F, 0x20, 0x62, 0x65, 0x20, 0x63, 0x75, 0x74,
    0x20, 0x61, 0x64, 0x72, 0x69, 0x66, 0x74, 0x0A,
    0x49, 0x27, 0x76, 0x65, 0x20, 0x62, 0x65, 0x65,
    0x6E, 0x20, 0x70, 0x61, 0x69, 0x64, 0x2C, 0x20,
    0x49, 0x27, 0x76, 0x65, 0x20, 0x62, 0x65, 0x65,
    0x6E, 0x20, 0x70, 0x61, 0x69, 0x64
};

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class SynchronizerSuite : public testing::Test
{
protected:
    using Algorithms = Configuration::Options::Algorithms;
    using SupportedAlgorithms = Configuration::Options::SupportedAlgorithms;
    using SynchronizationResult = std::pair<std::unique_ptr<Security::CipherPackage>, std::unique_ptr<Security::CipherPackage>>;

    std::shared_ptr<SupportedAlgorithms> SetupBasicSupportedAlgorithms()
    {
        return std::make_shared<SupportedAlgorithms>(
            SupportedAlgorithms{
                {
                    {
                        Security::ConfidentialityLevel::High,
                        Algorithms{ "high", { test::KeyAgreementName }, { test::CipherName }, { test::HashFunctionName } }
                    }
                }
            }
        );
    }

    std::pair<Security::PackageSynchronizer, Security::PackageSynchronizer> CreateSynchronizers(
        std::shared_ptr<SupportedAlgorithms> const& spInitiatorSupportedAlgorithms,
        std::shared_ptr<SupportedAlgorithms> const& spAcceptorSupportedAlgorithms)
    {
        // Explicitly cache the initiator's supported algorithms. A packed version of the of the supported algorithms is
        // cached in a static buffer such that every initiator that will be created does not have to duplicate the packing. 
        // However, by creating multiple configurations in the test, the wrong configuration may be packed. This should 
        // ensure the correct one is used in the calling test. 
        Security::PackageSynchronizer::PackAndCacheSupportedAlgorithms(*spInitiatorSupportedAlgorithms);

        return std::make_pair(
            Security::PackageSynchronizer{ Security::ExchangeRole::Initiator, spInitiatorSupportedAlgorithms },
            Security::PackageSynchronizer{ Security::ExchangeRole::Acceptor, spAcceptorSupportedAlgorithms });
    }

    SynchronizationResult PerformAndVerifySynchronization(Security::PackageSynchronizer& initiator, Security::PackageSynchronizer& acceptor)
    {
        auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = initiator.Initialize();
        [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = acceptor.Initialize();

        auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = acceptor.Synchronize(initiatorStageZeroBuffer);
        EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Processing);
        EXPECT_FALSE(acceptorStageOneBuffer.empty());

        auto const [initiatorStageOneStatus, initiatorStageOneBuffer] = initiator.Synchronize(acceptorStageOneBuffer);
        EXPECT_EQ(initiatorStageOneStatus, Security::SynchronizationStatus::Processing);
        EXPECT_FALSE(initiatorStageOneBuffer.empty());

        auto const [acceptorStageTwoStatus, acceptorStageTwoBuffer] = acceptor.Synchronize(initiatorStageOneBuffer);
        EXPECT_EQ(acceptorStageTwoStatus, Security::SynchronizationStatus::Ready);
        EXPECT_FALSE(acceptorStageTwoBuffer.empty());

        auto const [initiatorStageTwoStatus, initiatorStageTwoBuffer] = initiator.Synchronize(acceptorStageTwoBuffer);
        EXPECT_EQ(initiatorStageTwoStatus, Security::SynchronizationStatus::Ready);
        EXPECT_TRUE(initiatorStageTwoBuffer.empty());

        auto upInitiatorPackage = initiator.Finalize();
        EXPECT_TRUE(upInitiatorPackage);
        if (!upInitiatorPackage) {
            return { nullptr, nullptr };
        }
        EXPECT_FALSE(initiator.Finalize()); // The package should only be extractable once. 

        auto upAcceptorPackage = acceptor.Finalize();
        EXPECT_TRUE(upAcceptorPackage);
        if (!upAcceptorPackage) {
            return { nullptr, nullptr };
        }
        EXPECT_FALSE(acceptor.Finalize()); // The package should only be extractable once. 

        VerifyWorkingCipherPackages(upInitiatorPackage, upAcceptorPackage);

        return { std::move(upInitiatorPackage), std::move(upAcceptorPackage) };
    }

    void VerifyWorkingCipherPackages(
        std::unique_ptr<Security::CipherPackage> const& upInitiatorPackage,
        std::unique_ptr<Security::CipherPackage> const& upAcceptorPackage)
    {
        auto const optInitiatorEncrypted = upInitiatorPackage->Encrypt(test::Data);
        ASSERT_TRUE(optInitiatorEncrypted);

        auto const optAcceptorDecrypted = upAcceptorPackage->Decrypt(*optInitiatorEncrypted);
        ASSERT_TRUE(optAcceptorDecrypted);
        EXPECT_EQ(*optAcceptorDecrypted, test::Data);

        auto const optAcceptorEncrypted = upAcceptorPackage->Encrypt(test::Data);
        ASSERT_TRUE(optAcceptorEncrypted);
        EXPECT_NE(optInitiatorEncrypted, optAcceptorEncrypted);

        auto const optInitiatorDecrypted = upInitiatorPackage->Decrypt(*optAcceptorEncrypted);
        ASSERT_TRUE(optInitiatorDecrypted);
        EXPECT_EQ(*optInitiatorDecrypted, test::Data);
    
        {
            Security::Buffer buffer = test::Data;
            EXPECT_TRUE(upInitiatorPackage->Sign(buffer));
            EXPECT_EQ(buffer.size(), test::Data.size() + upInitiatorPackage->GetSuite().GetSignatureSize());

            EXPECT_EQ(upAcceptorPackage->Verify(buffer), Security::VerificationStatus::Success);
        }

        {
            Security::Buffer buffer = test::Data;
            EXPECT_TRUE(upAcceptorPackage->Sign(buffer));
            EXPECT_EQ(buffer.size(), test::Data.size() + upAcceptorPackage->GetSuite().GetSignatureSize());

            EXPECT_EQ(upInitiatorPackage->Verify(buffer), Security::VerificationStatus::Success);
        }
    }

    void VerifyNotWorkingCipherPackages(
        std::unique_ptr<Security::CipherPackage> const& upInitiatorPackage,
        std::unique_ptr<Security::CipherPackage> const& upAcceptorPackage)
    {
        auto const optInitiatorEncrypted = upInitiatorPackage->Encrypt(test::Data);
        ASSERT_TRUE(optInitiatorEncrypted);

        auto const optAcceptorDecrypted = upAcceptorPackage->Decrypt(*optInitiatorEncrypted);
        ASSERT_TRUE(optAcceptorDecrypted);
        EXPECT_NE(*optAcceptorDecrypted, test::Data);

        auto const optAcceptorEncrypted = upAcceptorPackage->Encrypt(test::Data);
        ASSERT_TRUE(optAcceptorEncrypted);
        EXPECT_NE(optInitiatorEncrypted, optAcceptorEncrypted);

        auto const optInitiatorDecrypted = upInitiatorPackage->Decrypt(*optAcceptorEncrypted);
        ASSERT_TRUE(optInitiatorDecrypted);
        EXPECT_NE(*optInitiatorDecrypted, test::Data);
    
        {
            Security::Buffer buffer = test::Data;
            EXPECT_TRUE(upInitiatorPackage->Sign(buffer));
            EXPECT_EQ(buffer.size(), test::Data.size() + upInitiatorPackage->GetSuite().GetSignatureSize());

            EXPECT_EQ(upAcceptorPackage->Verify(buffer), Security::VerificationStatus::Failed);
        }

        {
            Security::Buffer buffer = test::Data;
            EXPECT_TRUE(upAcceptorPackage->Sign(buffer));
            EXPECT_EQ(buffer.size(), test::Data.size() + upAcceptorPackage->GetSuite().GetSignatureSize());

            EXPECT_EQ(upInitiatorPackage->Verify(buffer), Security::VerificationStatus::Failed);
        }
    }
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, InitiatorSyncrhonizerSetupTest)
{
    auto const spSupportedAlgorithms = SetupBasicSupportedAlgorithms();

    // Note: This method must be called to cache the supported algorithms cache pack for all deployed synchronizers. 
    // This is done when the cipher service is first created, but this test does not make use of one. 
    Security::PackageSynchronizer::PackAndCacheSupportedAlgorithms(*spSupportedAlgorithms);

    Security::PackageSynchronizer synchronizer{ Security::ExchangeRole::Initiator, spSupportedAlgorithms };
    EXPECT_EQ(synchronizer.GetExchangeRole(), Security::ExchangeRole::Initiator);
    EXPECT_GT(synchronizer.GetStages(), std::size_t{ 0 });
    EXPECT_EQ(synchronizer.GetStatus(), Security::SynchronizationStatus::Processing);
    EXPECT_FALSE(synchronizer.Synchronized());
    EXPECT_FALSE(synchronizer.Finalize());

    auto const [status, buffer] = synchronizer.Initialize();
    EXPECT_EQ(status, Security::SynchronizationStatus::Processing); 
    EXPECT_FALSE(buffer.empty()); // An initiating synchronizer should return a non-empty request buffer to be sent to a peer. 

    EXPECT_EQ(synchronizer.GetStatus(), Security::SynchronizationStatus::Processing);
    EXPECT_FALSE(synchronizer.Synchronized());
    EXPECT_FALSE(synchronizer.Finalize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, AcceptorSyncrhonizerSetupTest)
{
    auto const spSupportedAlgorithms = SetupBasicSupportedAlgorithms();

    // Note: This method must be called to cache the supported algorithms cache pack for all deployed synchronizers. 
    // This is done when the cipher service is first created, but this test does not make use of one. 
    Security::PackageSynchronizer::PackAndCacheSupportedAlgorithms(*spSupportedAlgorithms);

    Security::PackageSynchronizer synchronizer{ Security::ExchangeRole::Acceptor, spSupportedAlgorithms };
    EXPECT_EQ(synchronizer.GetExchangeRole(), Security::ExchangeRole::Acceptor);
    EXPECT_GT(synchronizer.GetStages(), std::size_t{ 0 });
    EXPECT_EQ(synchronizer.GetStatus(), Security::SynchronizationStatus::Processing);
    EXPECT_FALSE(synchronizer.Synchronized());
    EXPECT_FALSE(synchronizer.Finalize());

    auto const [status, buffer] = synchronizer.Initialize();
    EXPECT_EQ(status, Security::SynchronizationStatus::Processing);
    EXPECT_TRUE(buffer.empty()); // An accepting synchronizer should not return any additional data to be sent to the peer. 

    EXPECT_EQ(synchronizer.GetStatus(), Security::SynchronizationStatus::Processing);
    EXPECT_FALSE(synchronizer.Synchronized());
    EXPECT_FALSE(synchronizer.Finalize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, BasicSynchronizationTest)
{
    auto const spSupportedAlgorithms = SetupBasicSupportedAlgorithms();
    auto&& [initiator, acceptor] = CreateSynchronizers(spSupportedAlgorithms, spSupportedAlgorithms);
    PerformAndVerifySynchronization(initiator, acceptor);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, HighConfidentialityMatchSynchronizationTest)
{
    auto const spInitiatorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "kem-frodokem-1344-shake", "kem-kyber1024", "kem-hqc-256", "kem-classic-mceliece-8192128f" },
                        { "aes-256-gcm", "aes-256-ocb", "aes-256-ccm", "chacha20-poly1305", "aria-256-gcm", "aria-256-ccm" },
                        { "blake2b512", "sha3-512", "sha512" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ecdh-secp-521-r1", "ffdhe-8192", "ecdh-secp-384-r1", "ffdhe-6144" },
                        { "aes-192-gcm", "aes-192-ocb", "aes-192-ccm", "aria-192-gcm", "aria-192-ccm", "aes-128-gcm", "aes-128-ccm", "aes-192-cbc" },
                        { "blake2s256", "sha3-384", "sha3-256", "sha512-256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-256-k1", "ffdhe-4096", "ecdh-secp-224-r1", "ffdhe-3072" },
                        { "des-ede3-cbc", "des-ede3-cfb", "des-ede3-cfb1", "des-ede3-cfb8", "des-ede3-ecb", "des-ede3-ofb", "des-ede-cbc" },
                        { "sha3-224", "sha224", "ripemd160", "sha1" }
                    }
                }
            }
        });

    auto const spAcceptorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "kem-frodokem-1344-shake", "kem-kyber1024", "kem-classic-mceliece-8192128f", "kem-hqc-256" },
                        { "aria-256-ccm", "aria-256-gcm", "aes-256-gcm", "aes-256-ccm", "chacha20-poly1305", "aes-256-ocb" },
                        { "sha3-512", "blake2b512", "sha512" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ecdh-secp-521-r1", "ecdh-secp-384-r1", "ffdhe-8192", "ffdhe-6144" },
                        { "aes-192-ocb", "aes-128-ccm", "aria-192-gcm", "aes-192-gcm", "aes-192-ccm", "aria-192-ccm", "aes-192-cbc", "aes-128-gcm" },
                        { "sha3-256", "sha3-384", "sha512-256", "blake2s256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-224-r1", "ffdhe-3072", "ecdh-secp-256-k1", "ffdhe-4096" },
                        { "des-ede3-cfb", "des-ede3-cbc", "des-ede3-cfb1", "des-ede3-ofb", "des-ede3-cfb8", "des-ede-cbc", "des-ede3-ecb" },
                        { "sha1", "ripemd160", "sha224", "sha3-224" }
                    }
                }
            }
        });

    auto&& [initiator, acceptor] = CreateSynchronizers(spInitiatorSupportedAlgorithms, spAcceptorSupportedAlgorithms);

    auto const& [upInitiatorPackage, upAcceptorPackage] = PerformAndVerifySynchronization(initiator, acceptor);
    ASSERT_TRUE(upInitiatorPackage && upAcceptorPackage);

    // Verify the package was selected based on the highest supported algorithms listed in the initiators priority order
    // was selected as a result of synchronization. 
    auto const& suite = upInitiatorPackage->GetSuite();
    EXPECT_EQ(suite.GetConfidentialityLevel(), Security::ConfidentialityLevel::High);
    EXPECT_EQ(suite.GetKeyAgreementName(), "kem-frodokem-1344-shake");
    EXPECT_EQ(suite.GetCipherName(), "aria-256-ccm");
    EXPECT_EQ(suite.GetHashFunctionName(), "sha3-512");

    EXPECT_EQ(suite, upAcceptorPackage->GetSuite());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, MediumConfidentialityMatchSynchronizationTest)
{
    auto const spInitiatorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "kem-frodokem-1344-shake", "kem-kyber1024", "kem-hqc-256", "kem-classic-mceliece-8192128f" },
                        { "aes-256-gcm", "aes-256-ocb", "aes-256-ccm", "chacha20-poly1305", "aria-256-gcm", "aria-256-ccm" },
                        { "blake2b512", "sha3-512", "shake256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ecdh-secp-521-r1", "ffdhe-8192", "ecdh-secp-384-r1", "ffdhe-6144" },
                        { "aes-192-gcm", "aes-192-ocb", "aes-192-ccm", "aria-192-gcm", "aria-192-ccm", "aes-128-gcm", "aes-128-ccm", "aes-192-cbc" },
                        { "blake2s256", "sha3-384", "sha3-256", "sha512-256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-256-k1", "ffdhe-4096", "ecdh-secp-224-r1", "ffdhe-3072" },
                        { "des-ede3-cbc", "des-ede3-cfb", "des-ede3-cfb1", "des-ede3-cfb8", "des-ede3-ecb", "des-ede3-ofb", "des-ede-cbc" },
                        { "sha3-224", "sha224", "ripemd160", "sha1" }
                    }
                }
            }
        });

    auto const spAcceptorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "ffdhe-6144", "ffdhe-8192", "ecdh-secp-224-k1", "ecdh-secp-224-r1" },
                        { "camellia-256-cfb", "camellia-256-ctr", "camellia-256-ofb", "chacha20" },
                        { "sha512" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ecdh-secp-521-r1", "ecdh-secp-384-r1", "ffdhe-8192", "ffdhe-6144" },
                        { "aes-192-ocb", "aes-128-ccm", "aria-192-gcm", "aes-192-gcm", "aes-192-ccm", "aria-192-ccm", "aes-192-cbc", "aes-128-gcm" },
                        { "sha3-256", "sha3-384", "sha512-256", "blake2s256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-224-r1", "ffdhe-3072", "ecdh-secp-256-k1", "ffdhe-4096" },
                        { "des-ede3-cfb", "des-ede3-cbc", "des-ede3-cfb1", "des-ede3-ofb", "des-ede3-cfb8", "des-ede-cbc", "des-ede3-ecb" },
                        { "sha1", "ripemd160", "sha224", "sha3-224" }
                    }
                }
            }
        });

    auto&& [initiator, acceptor] = CreateSynchronizers(spInitiatorSupportedAlgorithms, spAcceptorSupportedAlgorithms);

    auto const& [upInitiatorPackage, upAcceptorPackage] = PerformAndVerifySynchronization(initiator, acceptor);
    ASSERT_TRUE(upInitiatorPackage && upAcceptorPackage);

    // Verify the package was selected based on the highest supported algorithms listed in the initiators priority order
    // was selected as a result of synchronization. 
    auto const& suite = upInitiatorPackage->GetSuite();
    EXPECT_EQ(suite.GetConfidentialityLevel(), Security::ConfidentialityLevel::Medium);
    EXPECT_EQ(suite.GetKeyAgreementName(), "ffdhe-6144");
    EXPECT_EQ(suite.GetCipherName(), "aes-192-ocb");
    EXPECT_EQ(suite.GetHashFunctionName(), "sha3-256");

    EXPECT_EQ(suite, upAcceptorPackage->GetSuite());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, LowConfidentialityMatchSynchronizationTest)
{
    auto const spInitiatorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "kem-frodokem-1344-shake", "kem-kyber1024", "kem-hqc-256", "kem-classic-mceliece-8192128f" },
                        { "aes-256-gcm", "aes-256-ocb", "aes-256-ccm", "chacha20-poly1305", "aria-256-gcm", "aria-256-ccm" },
                        { "blake2b512", "sha3-512", "shake256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ecdh-secp-521-r1", "ffdhe-8192", "ecdh-secp-384-r1", "ffdhe-6144" },
                        { "aes-192-gcm", "aes-192-ocb", "aes-192-ccm", "aria-192-gcm", "aria-192-ccm", "aes-128-gcm", "aes-128-ccm", "aes-192-cbc" },
                        { "blake2s256", "sha3-384", "sha3-256", "sha512-256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-256-k1", "ffdhe-4096", "ecdh-secp-224-r1", "ffdhe-3072" },
                        { "des-ede3-cbc", "des-ede3-cfb", "des-ede3-cfb1", "des-ede3-cfb8", "des-ede3-ecb", "des-ede3-ofb", "des-ede-cbc" },
                        { "sha3-224", "sha224", "ripemd160", "sha1" }
                    }
                }
            }
        });

    auto const spAcceptorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "ecdh-sect-283-k1", "ecdh-sect-283-r1", "ecdh-secp-224-k1", "ecdh-secp-224-r1" },
                        { "camellia-256-cfb", "camellia-256-ctr", "camellia-256-ofb", "chacha20" },
                        { "sha512" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ffdhe-2048", "ecdh-secp-192-k1", "ecdh-secp-160-r1", "ecdh-secp-160-r2" },
                        { "aes-128-cbc", "aes-128-ctr", "aes-128-ofb", "aes-128-cfb" },
                        { "sha512-224", "sha384", "sha256", "sm3" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-224-r1", "ffdhe-3072", "ecdh-secp-256-k1", "ffdhe-4096" },
                        { "des-ede3-cfb", "des-ede3-cbc", "des-ede3-cfb1", "des-ede3-ofb", "des-ede3-cfb8", "des-ede-cbc", "des-ede3-ecb" },
                        { "sha1", "ripemd160", "sha224", "sha3-224" }
                    }
                }
            }
        });

    auto&& [initiator, acceptor] = CreateSynchronizers(spInitiatorSupportedAlgorithms, spAcceptorSupportedAlgorithms);

    auto const& [upInitiatorPackage, upAcceptorPackage] = PerformAndVerifySynchronization(initiator, acceptor);
    ASSERT_TRUE(upInitiatorPackage && upAcceptorPackage);

    // Verify the package was selected based on the highest supported algorithms listed in the initiators priority order
    // was selected as a result of synchronization. 
    auto const& suite = upInitiatorPackage->GetSuite();
    EXPECT_EQ(suite.GetConfidentialityLevel(), Security::ConfidentialityLevel::Low);
    EXPECT_EQ(suite.GetKeyAgreementName(), "ecdh-secp-224-r1");
    EXPECT_EQ(suite.GetCipherName(), "des-ede3-cfb");
    EXPECT_EQ(suite.GetHashFunctionName(), "sha1");

    EXPECT_EQ(suite, upAcceptorPackage->GetSuite());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, LargeSupportedAlgorithmsSynchronizationTest)
{
    auto const spSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "ffdhe-3072", "ffdhe-4096", "ffdhe-6144", "ffdhe-8192", "ecdh-secp-224-k1", "ecdh-secp-224-r1", "ecdh-secp-256-k1", "ecdh-secp-384-r1", "ecdh-secp-521-r1", "ecdh-sect-283-k1", "ecdh-sect-283-r1", "ecdh-sect-409-k1", "ecdh-sect-409-r1", "ecdh-sect-571-k1", "ecdh-sect-571-r1", "ecdh-brainpool-p-256-r1", "ecdh-brainpool-p-256-t1", "ecdh-brainpool-p-320-r1", "ecdh-brainpool-p-320-t1", "ecdh-brainpool-p-384-r1", "ecdh-brainpool-p-384-t1", "ecdh-brainpool-p-512-r1", "ecdh-brainpool-p-512-t1", "kem-bike-l3", "kem-bike-l5", "kem-classic-mceliece-6688128", "kem-classic-mceliece-6688128f", "kem-classic-mceliece-6960119", "kem-classic-mceliece-6960119f", "kem-classic-mceliece-8192128", "kem-classic-mceliece-8192128f", "kem-hqc-192", "kem-hqc-256", "kem-kyber768", "kem-kyber1024", "kem-sntruprime-sntrup761", "kem-frodokem-976-aes", "kem-frodokem-976-shake", "kem-frodokem-1344-aes", "kem-frodokem-1344-shake" },
                        { "aes-256-gcm", "aes-256-ocb", "aes-256-ccm", "chacha20-poly1305", "aria-256-gcm", "aria-256-ccm", "aes-256-cbc", "aes-256-ctr", "aes-256-ofb", "aria-256-cbc", "aria-256-cfb", "aria-256-cfb1", "aria-256-cfb8", "aria-256-ctr", "aria-256-ofb", "camellia-256-cbc", "camellia-256-cfb", "camellia-256-cfb1", "camellia-256-cfb8", "camellia-256-ctr", "camellia-256-ofb", "chacha20" },
                        { "blake2b512", "sha3-512", "sha512" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ffdhe-2048", "ecdh-secp-192-k1", "ecdh-secp-160-r1", "ecdh-secp-160-r2", "ecdh-prime-192-v1", "ecdh-prime-192-v2", "ecdh-prime-192-v3", "ecdh-prime-239-v1", "ecdh-prime-239-v2", "ecdh-prime-239-v3", "ecdh-prime-256-v1", "ecdh-sect-233-k1", "ecdh-sect-233-r1", "ecdh-sect-239-k1", "ecdh-c2tnb-359-v1", "ecdh-c2pnb-368-w1", "ecdh-c2tnb-431-r1", "kem-bike-l1", "kem-classic-mceliece-348864", "kem-classic-mceliece-348864f", "kem-classic-mceliece-460896", "kem-classic-mceliece-460896f", "kem-hqc-128", "kem-kyber512", "kem-frodokem-640-aes", "kem-frodokem-640-shake" },
                        { "aes-192-gcm", "aes-192-ocb", "aes-192-ccm", "aria-192-gcm", "aria-192-ccm", "aes-128-gcm", "aes-128-ccm", "aes-192-cbc", "aes-192-ctr", "aes-192-ofb",  "aes-128-cbc", "aes-128-ctr", "aes-128-ofb", "aes-128-cfb", "aes-128-cfb1", "aes-128-cfb8", "aria-192-cbc", "aria-192-cfb", "aria-192-cfb1", "aria-192-cfb8", "aria-192-ctr", "aria-192-ofb", "camellia-192-cbc", "camellia-192-cfb", "camellia-192-cfb1", "camellia-192-cfb8", "camellia-192-ctr", "camellia-192-ofb" },
                        { "blake2s256", "sha3-384", "sha3-256", "sha512-256", "sha512-224", "sha384", "sha256", "sm3" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-128-r1", "ecdh-secp-128-r2", "ecdh-sect-131-r1", "ecdh-sect-131-r2", "ecdh-sect-163-k1", "ecdh-sect-163-r1", "ecdh-sect-163-r2", "ecdh-sect-193-r1", "ecdh-sect-193-r2", "ecdh-c2pnb-163-v1", "ecdh-c2pnb-163-v2", "ecdh-c2pnb-163-v3", "ecdh-c2pnb-176-v1", "ecdh-c2tnb-191-v1", "ecdh-c2tnb-191-v2", "ecdh-c2tnb-191-v3", "ecdh-c2pnb-208-w1", "ecdh-c2tnb-239-v1", "ecdh-c2tnb-239-v2", "ecdh-c2tnb-239-v3", "ecdh-c2pnb-272-w1", "ecdh-c2pnb-304-w1", "ecdh-oakley-ec2n-3", "ecdh-oakley-ec2n-4", "ecdh-brainpool-p-160-r1", "ecdh-brainpool-p-160-t1", "ecdh-brainpool-p-192-r1", "ecdh-brainpool-p-192-t1", "ecdh-brainpool-p-224-r1", "ecdh-brainpool-p-224-t1" },
                        { "des-ede3-cbc", "des-ede3-cfb", "des-ede3-cfb1", "des-ede3-cfb8", "des-ede3-ecb", "des-ede3-ofb", "des-ede-cbc", "des-ede-cfb", "des-ede-ecb", "des-ede-ofb", "des-ede3", "sm4-ecb", "sm4-cbc", "sm4-cfb", "sm4-ofb", "sm4-ctr" },
                        { "sha3-224", "sha224", "ripemd160", "sha1", "md5-sha1" }
                    }
                }
            }
        });

    Security::PackageSynchronizer initiator{ Security::ExchangeRole::Initiator, spSupportedAlgorithms };
    Security::PackageSynchronizer acceptor{ Security::ExchangeRole::Acceptor, spSupportedAlgorithms };

    auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = initiator.Initialize();
    [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = acceptor.Initialize();

    auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = acceptor.Synchronize(initiatorStageZeroBuffer);
    EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Error);
    EXPECT_TRUE(acceptorStageOneBuffer.empty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, NoSupportedAlgorithmsSynchronizationTest)
{
    auto const spInitiatorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "kem-frodokem-1344-shake", "kem-kyber1024", "kem-hqc-256", "kem-classic-mceliece-8192128f" },
                        { "aes-256-gcm", "aes-256-ocb", "aes-256-ccm", "chacha20-poly1305", "aria-256-gcm", "aria-256-ccm" },
                        { "blake2b512", "sha3-512", "sha512" }
                    }
                }
            }
        });

    auto const spAcceptorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ecdh-secp-521-r1", "ecdh-secp-384-r1", "ffdhe-8192", "ffdhe-6144" },
                        { "aes-192-ocb", "aes-128-ccm", "aria-192-gcm", "aes-192-gcm", "aes-192-ccm", "aria-192-ccm", "aes-192-cbc", "aes-128-gcm" },
                        { "sha3-256", "sha3-384", "sha512-256", "blake2s256" }
                    }
                }
            }
        });

    auto&& [initiator, acceptor] = CreateSynchronizers(spInitiatorSupportedAlgorithms, spAcceptorSupportedAlgorithms);

    auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = initiator.Initialize();
    [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = acceptor.Initialize();

    auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = acceptor.Synchronize(initiatorStageZeroBuffer);
    EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Error);
    EXPECT_TRUE(acceptorStageOneBuffer.empty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, DifferingConfidentialityLevelSynchronizationTest)
{
    auto const spHighSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{
                        "high",
                        { "kem-frodokem-1344-shake", "kem-kyber1024", "kem-hqc-256", "kem-classic-mceliece-8192128f" },
                        { "aes-256-gcm", "aes-256-ocb", "aes-256-ccm", "chacha20-poly1305", "aria-256-gcm", "aria-256-ccm" },
                        { "blake2b512", "sha3-512", "sha512" }
                    }
                }
            }
        });

    auto const spMediumSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "high", 
                        { "kem-frodokem-1344-shake", "kem-kyber1024", "kem-classic-mceliece-8192128f", "kem-hqc-256" },
                        { "aria-256-ccm", "aria-256-gcm", "aes-256-gcm", "aes-256-ccm", "chacha20-poly1305", "aes-256-ocb" },
                        { "sha3-512", "blake2b512", "sha512" }
                    }
                }
            }
        });

    // Perform the test with the initiator matching at the high confidentiality level. 
    {
        auto [initiator, acceptor] = CreateSynchronizers(spHighSupportedAlgorithms, spMediumSupportedAlgorithms);

        auto const& [upInitiatorPackage, upAcceptorPackage] = PerformAndVerifySynchronization(initiator, acceptor);
        ASSERT_TRUE(upInitiatorPackage && upAcceptorPackage);

        // Verify the package was selected based on the highest supported algorithms listed in the initiators priority order
        // was selected as a result of synchronization. 
        auto const& initiatorSuite = upInitiatorPackage->GetSuite();
        EXPECT_EQ(initiatorSuite.GetConfidentialityLevel(), Security::ConfidentialityLevel::High);
        EXPECT_EQ(initiatorSuite.GetKeyAgreementName(), "kem-frodokem-1344-shake");
        EXPECT_EQ(initiatorSuite.GetCipherName(), "aria-256-ccm");
        EXPECT_EQ(initiatorSuite.GetHashFunctionName(), "sha3-512");

        auto const& acceptorSuite = upAcceptorPackage->GetSuite();
        EXPECT_NE(initiatorSuite, acceptorSuite);
        EXPECT_EQ(acceptorSuite.GetConfidentialityLevel(), Security::ConfidentialityLevel::Medium);
        EXPECT_EQ(acceptorSuite.GetKeyAgreementName(), initiatorSuite.GetKeyAgreementName());
        EXPECT_EQ(acceptorSuite.GetCipherName(), initiatorSuite.GetCipherName());
        EXPECT_EQ(acceptorSuite.GetHashFunctionName(), initiatorSuite.GetHashFunctionName());
    }

    // Perform the test with the initiator matching at the medium confidentiality level. 
    {
        auto [initiator, acceptor] = CreateSynchronizers(spMediumSupportedAlgorithms, spHighSupportedAlgorithms);

        auto const& [upInitiatorPackage, upAcceptorPackage] = PerformAndVerifySynchronization(initiator, acceptor);
        ASSERT_TRUE(upInitiatorPackage && upAcceptorPackage);

        // Verify the package was selected based on the highest supported algorithms listed in the initiators priority order
        // was selected as a result of synchronization. 
        auto const& initiatorSuite = upInitiatorPackage->GetSuite();
        EXPECT_EQ(initiatorSuite.GetConfidentialityLevel(), Security::ConfidentialityLevel::Medium);
        EXPECT_EQ(initiatorSuite.GetKeyAgreementName(), "kem-frodokem-1344-shake");
        EXPECT_EQ(initiatorSuite.GetCipherName(), "aes-256-gcm");
        EXPECT_EQ(initiatorSuite.GetHashFunctionName(), "blake2b512");

        auto const& acceptorSuite = upAcceptorPackage->GetSuite();
        EXPECT_NE(initiatorSuite, acceptorSuite);
        EXPECT_EQ(acceptorSuite.GetConfidentialityLevel(), Security::ConfidentialityLevel::High);
        EXPECT_EQ(acceptorSuite.GetKeyAgreementName(), initiatorSuite.GetKeyAgreementName());
        EXPECT_EQ(acceptorSuite.GetCipherName(), initiatorSuite.GetCipherName());
        EXPECT_EQ(acceptorSuite.GetHashFunctionName(), initiatorSuite.GetHashFunctionName());
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, MatchingAlgorithmsAtDifferentLevelsSynchronizationTest)
{
    auto const spInitiatorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "kem-frodokem-1344-shake", "kem-kyber1024", "kem-hqc-256", "kem-classic-mceliece-8192128f" },
                        { "aes-256-gcm", "aes-256-ocb", "aes-256-ccm", "chacha20-poly1305", "aria-256-gcm", "aria-256-ccm" },
                        { "blake2b512", "sha3-512", "shake256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ecdh-secp-521-r1", "ffdhe-8192", "ecdh-secp-384-r1", "ffdhe-6144" },
                        { "aes-192-gcm", "aes-192-ocb", "aes-192-ccm", "aria-192-gcm", "aria-192-ccm", "aes-128-gcm", "aes-128-ccm", "aes-192-cbc" },
                        { "blake2s256", "sha3-384", "sha3-256", "sha512-256" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-256-k1", "ffdhe-4096", "ecdh-secp-224-r1", "ffdhe-3072" },
                        { "des-ede3-cbc", "des-ede3-cfb", "des-ede3-cfb1", "des-ede3-cfb8", "des-ede3-ecb", "des-ede3-ofb", "des-ede-cbc" },
                        { "sha3-224", "sha224", "ripemd160", "sha1" }
                    }
                }
            }
        });

    auto const spAcceptorSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::High,
                    Algorithms{ 
                        "high", 
                        { "ecdh-sect-283-k1", "ecdh-sect-283-r1", "ecdh-secp-224-k1", "ecdh-secp-224-r1" },
                        { "aria-256-ccm", "aria-256-gcm", "aes-256-gcm", "aes-256-ccm", "chacha20-poly1305", "aes-256-ocb" },
                        { "sha512" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Medium,
                    Algorithms{ 
                        "medium", 
                        { "ecdh-secp-521-r1", "ecdh-secp-384-r1", "ffdhe-8192", "ffdhe-6144" },
                        { "aes-128-cbc", "aes-128-ctr", "aes-128-ofb", "aes-128-cfb" },
                        { "sha512-224", "sha384", "sha256", "sm3" }
                    }
                },
                {
                    Security::ConfidentialityLevel::Low,
                    Algorithms{ 
                        "low", 
                        { "ecdh-secp-224-r1", "ffdhe-3072", "ecdh-secp-256-k1", "ffdhe-4096" },
                        { "des-ede3-cfb", "des-ede3-cbc", "des-ede3-cfb1", "des-ede3-ofb", "des-ede3-cfb8", "des-ede-cbc", "des-ede3-ecb" },
                        { "sha1", "ripemd160", "sha224", "sha3-224" }
                    }
                }
            }
        });

    auto&& [initiator, acceptor] = CreateSynchronizers(spInitiatorSupportedAlgorithms, spAcceptorSupportedAlgorithms);

    auto const& [upInitiatorPackage, upAcceptorPackage] = PerformAndVerifySynchronization(initiator, acceptor);
    ASSERT_TRUE(upInitiatorPackage && upAcceptorPackage);

    // Verify the package was selected based on the highest supported algorithms listed in the initiators priority order
    // was selected as a result of synchronization. 
    auto const& suite = upInitiatorPackage->GetSuite();
    EXPECT_EQ(suite.GetConfidentialityLevel(), Security::ConfidentialityLevel::Low);
    EXPECT_EQ(suite.GetKeyAgreementName(), "ecdh-secp-224-r1");
    EXPECT_EQ(suite.GetCipherName(), "aria-256-ccm");
    EXPECT_EQ(suite.GetHashFunctionName(), "sha1");
    EXPECT_EQ(suite, upAcceptorPackage->GetSuite());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, UnknownConfidentialityLevelSynchronizationTest)
{
    auto const spUnknownSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
        SupportedAlgorithms{
            {
                {
                    Security::ConfidentialityLevel::Unknown,
                    Algorithms{
                        "", { test::KeyAgreementName }, { test::CipherName }, { test::HashFunctionName }
                    }
                }
            }
        });

    // Perform the test with the initiator missing the key agreement. 
    {
        auto&& [initiator, acceptor] = CreateSynchronizers(spUnknownSupportedAlgorithms, spUnknownSupportedAlgorithms);

        auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = initiator.Initialize();
        [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = acceptor.Initialize();

        auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = acceptor.Synchronize(initiatorStageZeroBuffer);
        EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Error);
        EXPECT_TRUE(acceptorStageOneBuffer.empty());
    }

    // Perform the test with the acceptor missing the key agreement. 
    {
        auto&& [initiator, acceptor] = CreateSynchronizers(spUnknownSupportedAlgorithms, spUnknownSupportedAlgorithms);

        auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = initiator.Initialize();
        [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = acceptor.Initialize();

        auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = acceptor.Synchronize(initiatorStageZeroBuffer);
        EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Error);
        EXPECT_TRUE(acceptorStageOneBuffer.empty());
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, MalformedSupportedAlgorithmsSynchronizationTest)
{
    auto const garbageName = [] () {
        auto const data = Security::Test::GenerateGarbageData(Security::MaximumSupportedAlgorithmNameSize);
        return std::string{ data.begin(), data.end() };
    }();

    using MalformedAlgorithms = std::vector<std::tuple<std::string, std::string, std::string>>;
    MalformedAlgorithms malformedAlgorithms = {
        { { "" }, { test::CipherName }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { "" }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { test::CipherName }, { "" } },
        { { }, { test::CipherName }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { test::CipherName }, { } },
        { { "unknown" }, { test::CipherName }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { "unknown" }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { test::CipherName }, { "unknown" } },
        { { "unknown" }, { test::CipherName }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { "unknown" }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { test::CipherName }, { "unknown" } },
        { { garbageName }, {test::CipherName}, {test::HashFunctionName}},
        { { test::KeyAgreementName }, { garbageName }, { test::HashFunctionName } },
        { { test::KeyAgreementName }, { test::CipherName }, { garbageName } },
    };
    
    for (auto const& [keyAgreement, cipher, hashFunction] : malformedAlgorithms) {
        auto const spBasicSupportedAlgorithms = SetupBasicSupportedAlgorithms();

        auto const spMalformedSupportedAlgorithms = std::make_shared<SupportedAlgorithms>(
            SupportedAlgorithms{
                {
                    {
                        Security::ConfidentialityLevel::High,
                        Algorithms{
                            "high", { keyAgreement }, { cipher }, { hashFunction }
                        }
                    }
                }
            });

        // Perform the test with the initiator missing the key agreement. 
        {
            auto&& [initiator, acceptor] = CreateSynchronizers(spMalformedSupportedAlgorithms, spBasicSupportedAlgorithms);

            auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = initiator.Initialize();
            [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = acceptor.Initialize();

            auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = acceptor.Synchronize(initiatorStageZeroBuffer);
            EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Error);
            EXPECT_TRUE(acceptorStageOneBuffer.empty());
        }

        // Perform the test with the acceptor missing the key agreement. 
        {
            auto&& [initiator, acceptor] = CreateSynchronizers(spBasicSupportedAlgorithms, spMalformedSupportedAlgorithms);

            auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = initiator.Initialize();
            [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = acceptor.Initialize();

            auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = acceptor.Synchronize(initiatorStageZeroBuffer);
            EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Error);
            EXPECT_TRUE(acceptorStageOneBuffer.empty());
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, MutatedAcceptorStageOneSynchronizationTest)
{
    struct MaliciousOverrides
    {
        std::optional<std::size_t> groupSize;
        std::optional<std::function<std::size_t(std::size_t const&)>> nameSize;
        bool useTrueByteCount{ true };
    };

    struct MaliciousKeyAgreement
    {
        std::string const name{ test::KeyAgreementName };
        MaliciousOverrides overrides;
    };

    struct MaliciousCipher
    {
        std::string const name{ test::CipherName };
        MaliciousOverrides overrides;
    };

    struct MaliciousHashFunction
    {
        std::string const name{ test::HashFunctionName };
        MaliciousOverrides overrides;
    };

    struct MaliciousPackage
    {
        MaliciousKeyAgreement keyAgreement;
        MaliciousCipher cipher;
        MaliciousHashFunction hashFunction;
    };

    using MaliciousMutations = std::vector<MaliciousOverrides>;
    MaliciousMutations mutations{
        { .groupSize = 0 },
        { .groupSize = 8 },
        { .groupSize = std::numeric_limits<std::uint16_t>::max() },
        { .nameSize = [] (std::size_t size) { return 0; } },
        { .nameSize = [] (std::size_t size) { return size / 2; } },
        { .nameSize = [] (std::size_t size) { return size * 2; } },
        { .nameSize =  [] (std::size_t size) { return std::numeric_limits<std::uint16_t>::max(); } }
    };

    enum MutationSelection : std::uint32_t { KeyAgreement, Cipher, HashFunction };

    using MaliclousSelections = std::vector<std::vector<MutationSelection>>;
    MaliclousSelections selections{
        { KeyAgreement },
        { Cipher },
        { HashFunction },
        { KeyAgreement, Cipher },
        { Cipher, HashFunction },
        { KeyAgreement, HashFunction },
        { KeyAgreement, HashFunction },
    };

    constexpr auto pack = [] (MaliciousPackage const& package) {
        constexpr auto packAlgorithm = [] (std::string const& name, MaliciousOverrides const& overrides, Security::Buffer& result) {
            constexpr auto calculateByteSize = [] (std::string const& name, MaliciousOverrides const& overrides) {
                auto const size = (!overrides.useTrueByteCount && overrides.nameSize) ? (*overrides.nameSize)(name.size()) : name.size();
                return size + sizeof(std::uint16_t); // Return the preceding size plus the element's size, add two bytes for the size prefix. 
            };

            PackUtils::PackChunk(static_cast<std::uint16_t>(overrides.groupSize ? *overrides.groupSize : 1), result);
            PackUtils::PackChunk(static_cast<std::uint16_t>(calculateByteSize(name, overrides)), result);
            PackUtils::PackChunk<std::uint16_t>((overrides.nameSize) ? (*overrides.nameSize)(name.size()) : name.size(), result);
            PackUtils::PackChunk(name, result);
        };
        
        Security::Buffer result;
        packAlgorithm(package.keyAgreement.name, package.keyAgreement.overrides, result);
        packAlgorithm(package.cipher.name, package.cipher.overrides, result);
        packAlgorithm(package.hashFunction.name, package.hashFunction.overrides, result);

        return result;
    };

    auto const spSupportedAlgorithms = SetupBasicSupportedAlgorithms();

    // Perform the test by misreporting the size of the key agreements as zero. 
    for (auto& overrides : mutations) {
        for (bool const useTrueByteCount : { true, false }) {
            for (auto const& selection : selections) {
                MaliciousPackage package;

                overrides.useTrueByteCount = useTrueByteCount;
                for (auto const& selected : selection) {
                    switch (selected) {
                        case KeyAgreement: { package.keyAgreement.overrides = overrides; } break;
                        case Cipher: { package.cipher.overrides = overrides; } break;
                        case HashFunction: { package.hashFunction.overrides = overrides; } break;
                    }
                }

                auto&& [initiator, acceptor] = CreateSynchronizers(spSupportedAlgorithms, spSupportedAlgorithms);

                auto const [initiatorStageZeroStatus, initiatorStageZeroBuffer] = initiator.Initialize();
                [[maybe_unused]] auto const [acceptorStageZeroStatus, acceptorStageZeroBuffer] = acceptor.Initialize();

                auto const malicious = pack(package);

                auto const [acceptorStageOneStatus, acceptorStageOneBuffer] = acceptor.Synchronize(malicious);
                EXPECT_EQ(acceptorStageOneStatus, Security::SynchronizationStatus::Error);
                EXPECT_TRUE(acceptorStageOneBuffer.empty());
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

// TODO: Tests covering malicious mutation of the other stage messages. 

//----------------------------------------------------------------------------------------------------------------------

TEST_F(SynchronizerSuite, GarabageDataSynchronizationTest)
{
    auto const garbageData = Security::Test::GenerateGarbageData(std::pow(2, 24));

    std::vector<double> inputs{
        0.0,
        std::pow(2, 0),
        std::pow(2, 2),
        std::pow(2, 8),
        std::pow(2, 16),
        std::pow(2, 24)
    };

    auto const spBasicSupportedAlgorithms = SetupBasicSupportedAlgorithms();
    
    // For each stage in the synchronizer process, test each stage by passing in garbage data. 
    for (std::uint32_t stage = 0; stage < 2; ++stage) {
        for (auto const inputSize : inputs) {
            auto&& [initiator, acceptor] = CreateSynchronizers(spBasicSupportedAlgorithms, spBasicSupportedAlgorithms);
            
            // Initialize the synchronizers and setup a buffer for each such that the they can be synchronized up to 
            // a specified stage for the test. 
            Security::Buffer initiatorBuffer = [&] () {
                auto&&[status, buffer] = initiator.Initialize();
                return buffer;
            }();

            Security::Buffer acceptorBuffer = [&] () {
                auto&&[status, buffer] = acceptor.Initialize();
                return buffer;
            }();

            // Run the synchronizer with valid input until we reach the stage to be tested. 
            for (std::uint32_t process = 0; process < stage; ++process) {
                acceptorBuffer = [&] () {
                    auto&&[status, buffer] = acceptor.Synchronize(initiatorBuffer);
                    EXPECT_EQ(status, Security::SynchronizationStatus::Processing);
                    EXPECT_FALSE(buffer.empty());
                    return buffer;
                }();

                initiatorBuffer = [&] () {
                    auto&&[status, buffer] = initiator.Synchronize(acceptorBuffer);
                    EXPECT_EQ(status, Security::SynchronizationStatus::Processing);
                    EXPECT_FALSE(buffer.empty());
                    return buffer;
                }();
            }

            // Verify that both synchronizers when the garbage input is provided. 
            Security::ReadableView garbageView{ garbageData.begin(), static_cast<std::size_t>(inputSize) };

            auto const [acceptorBadInputStatus, acceptorBadInputBuffer] = acceptor.Synchronize(garbageView);
            EXPECT_EQ(acceptorBadInputStatus, Security::SynchronizationStatus::Error);
            EXPECT_TRUE(acceptorBadInputBuffer.empty());

            auto const [initiatorBadInputStatus, initiatorBadInputBuffer] = initiator.Synchronize(garbageView);
            EXPECT_EQ(initiatorBadInputStatus, Security::SynchronizationStatus::Error);
            EXPECT_TRUE(initiatorBadInputBuffer.empty());
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
