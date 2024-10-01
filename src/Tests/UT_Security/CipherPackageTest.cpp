//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Security/Algorithms.hpp"
#include "Components/Security/CipherPackage.hpp"
#include "Components/Security/KeyStore.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

#pragma warning(disable : 4834)

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

constexpr Security::ConfidentialityLevel Level = Security::ConfidentialityLevel::High;
constexpr std::string_view KeyAgreementName = "basic-agreement";
constexpr std::string_view CipherName = "aes-256-ctr";
constexpr std::string_view HashFunctionName = "sha384";

auto const garbageData = Security::Test::GenerateGarbageData(std::pow(2, 24));

std::vector<double> MutationProbabilities{
    0.0,
    0.1,
    0.33,
    0.66,
    0.85, 
    0.95,
    1.0
};

void MutateBuffer(Security::WriteableView buffer, double probability);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class CipherPackageSuite : public testing::Test
{
protected:
    void SetupTestResources(std::string_view keyAgreement, std::string_view cipher, std::string_view hashFunction)
    {
        // Create the cipher suite to be used for the test run. 
        m_upCipherSuite = std::make_unique<Security::CipherSuite>(test::Level, keyAgreement, cipher, hashFunction);

        auto const initiatorPublicKey = Security::PublicKey{ Security::Test::GenerateGarbageData(256) };
        auto const acceptorPublicKey = Security::PublicKey{ Security::Test::GenerateGarbageData(256) };
        auto const sharedSecret = Security::SharedSecret{ Security::Test::GenerateGarbageData(256) };

        // Create the key store that will be used to create the cipher package 
        Security::KeyStore initiatorStore{ Security::PublicKey{ initiatorPublicKey } };
        initiatorStore.SetPeerPublicKey(Security::PublicKey{ acceptorPublicKey });

        // Create the key store that will be used to create the cipher package 
        Security::KeyStore acceptorStore{ Security::PublicKey{ acceptorPublicKey } };
        acceptorStore.SetPeerPublicKey(Security::PublicKey{ initiatorPublicKey });

        auto const initialInitiatorSalt = initiatorStore.GetSalt();
        initiatorStore.PrependSessionSalt(acceptorStore.GetSalt());
        acceptorStore.AppendSessionSalt(initialInitiatorSalt);

        initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, *m_upCipherSuite, sharedSecret);
        acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, *m_upCipherSuite, sharedSecret);

        m_upInitiatorPackage = std::make_unique<Security::CipherPackage>(*m_upCipherSuite, std::move(initiatorStore));
        m_upAcceptorPackage = std::make_unique<Security::CipherPackage>(*m_upCipherSuite, std::move(acceptorStore));
    }

    std::unique_ptr<Security::CipherSuite> m_upCipherSuite;
    std::unique_ptr<Security::CipherPackage> m_upInitiatorPackage;
    std::unique_ptr<Security::CipherPackage> m_upAcceptorPackage;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, BasicConstructorTest)
{
    SetupTestResources(test::KeyAgreementName, test::CipherName, test::HashFunctionName);

    EXPECT_EQ(m_upCipherSuite->GetConfidentialityLevel(), Security::ConfidentialityLevel::High);
    EXPECT_EQ(m_upCipherSuite->GetKeyAgreementName(), test::KeyAgreementName);
    EXPECT_EQ(m_upCipherSuite->GetCipherName(), test::CipherName);
    EXPECT_EQ(m_upCipherSuite->GetHashFunctionName(), test::HashFunctionName);
    EXPECT_EQ(m_upCipherSuite->GetEncryptionKeySize(), 32);
    EXPECT_EQ(m_upCipherSuite->GetInitializationVectorSize(), 16);
    EXPECT_FALSE(m_upCipherSuite->DoesCipherPadInput());
    EXPECT_FALSE(m_upCipherSuite->IsAuthenticatedCipher());
    EXPECT_TRUE(m_upCipherSuite->NeedsGeneratedInitializationVector());
    EXPECT_EQ(m_upCipherSuite->GetBlockSize(), 1);
    EXPECT_EQ(m_upCipherSuite->GetTagSize(), 0);
    EXPECT_EQ(m_upCipherSuite->GetSignatureKeySize(), 48);
    EXPECT_EQ(m_upCipherSuite->GetSignatureSize(), 48);
    
    ASSERT_TRUE(m_upCipherSuite->GetCipher());
    ASSERT_TRUE(m_upCipherSuite->GetDigest());

    EXPECT_EQ(m_upInitiatorPackage->GetSuite(), *m_upCipherSuite);

    auto data = Security::Test::GenerateGarbageData(256);
    EXPECT_TRUE(m_upInitiatorPackage->Encrypt(data));
    EXPECT_TRUE(m_upInitiatorPackage->Decrypt(data));
    EXPECT_TRUE(m_upInitiatorPackage->Sign(data));
    EXPECT_EQ(m_upAcceptorPackage->Verify(data), Security::VerificationStatus::Success);

    EXPECT_TRUE(m_upAcceptorPackage->Encrypt(data));
    EXPECT_TRUE(m_upAcceptorPackage->Decrypt(data));
    EXPECT_TRUE(m_upAcceptorPackage->Sign(data));
    EXPECT_EQ(m_upInitiatorPackage->Verify(data), Security::VerificationStatus::Success);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, EncryptionTest)
{
    using TestInputSizeGenerators = std::vector<std::function<std::size_t(Security::CipherSuite const&)>>;

    constexpr std::size_t MaximumTestInputSize = std::numeric_limits<std::uint16_t>::max() + 1;

    TestInputSizeGenerators generators = {
        [] (auto const&) { return 1; },
        [] (auto const& cipherSuite) { return std::max(cipherSuite.GetBlockSize() - 1, std::size_t{ 1 }); },
        [] (auto const& cipherSuite) { return cipherSuite.GetBlockSize(); },
        [] (auto const& cipherSuite) { return cipherSuite.GetBlockSize() + 1; },
        [] (auto const& cipherSuite) { return (cipherSuite.GetBlockSize() * 2) + (cipherSuite.GetBlockSize() / 2); },
        [] (auto const& cipherSuite) { return 100; },
        [] (auto const& cipherSuite) { return 255; },
        [] (auto const& cipherSuite) { return 256; },
        [] (auto const& cipherSuite) { return 512; },
        [] (auto const& cipherSuite) { return 1024; },
        [] (auto const& cipherSuite) { return 4096; },
        [] (auto const& cipherSuite) { return 16384; },
        [&] (auto const& cipherSuite) { return MaximumTestInputSize; },
    };

    auto const data = Security::Test::GenerateGarbageData(MaximumTestInputSize);
    for (auto const& cipherName : Security::SupportedCipherNames) {
        SetupTestResources(test::KeyAgreementName, cipherName, test::HashFunctionName);

        for (auto const& generator : generators) {
            auto const inputSize = generator(*m_upCipherSuite);

            std::span const dataview{ data.begin(), inputSize };

            auto const optInitiatorEncrypted = m_upInitiatorPackage->Encrypt(dataview);
            ASSERT_TRUE(optInitiatorEncrypted);
            EXPECT_EQ(optInitiatorEncrypted->size(), m_upCipherSuite->GetEncryptedSize(inputSize));

            auto const optAcceptorDecrypted = m_upAcceptorPackage->Decrypt(*optInitiatorEncrypted);
            ASSERT_TRUE(optAcceptorDecrypted);
            EXPECT_TRUE(std::ranges::equal(std::span{ optAcceptorDecrypted->data(), inputSize }, dataview));

            auto const optAcceptorEncrypted = m_upAcceptorPackage->Encrypt(dataview);
            ASSERT_TRUE(optAcceptorEncrypted);
            EXPECT_EQ(optAcceptorEncrypted->size(), m_upCipherSuite->GetEncryptedSize(inputSize));

            auto const optInitiatorDecrypted = m_upInitiatorPackage->Decrypt(*optAcceptorEncrypted);
            ASSERT_TRUE(optInitiatorDecrypted);
            EXPECT_TRUE(std::ranges::equal(std::span{ optInitiatorDecrypted->data(), inputSize }, dataview));
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, MaliciousEncryptionTest)
{
    constexpr auto TestDataSize = 2048;
    auto const data = Security::Test::GenerateGarbageData(TestDataSize);

    for (auto const& cipherName : Security::SupportedCipherNames) {
        SetupTestResources(test::KeyAgreementName, cipherName, test::HashFunctionName);

        for (auto const& probability : test::MutationProbabilities) {
            auto optEncrypted = m_upInitiatorPackage->Encrypt(data);
            ASSERT_TRUE(optEncrypted);

            test::MutateBuffer(*optEncrypted, probability);

            auto const optDecrypted = m_upAcceptorPackage->Decrypt(*optEncrypted);

            // Depending upon the algorithm the package may or may not return some decrypted data. 
            // If it does return some data, then we need to verify that it does not match the input data. 
            if (optDecrypted) {
                EXPECT_NE(*optDecrypted, data);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, SignatureTest)
{
    using TestInputSizeGenerators = std::vector<std::function<std::size_t(Security::CipherSuite const&)>>;

    constexpr std::size_t MaximumTestInputSize = std::numeric_limits<std::uint16_t>::max() + 1;

    TestInputSizeGenerators generators = {
        [] (auto const&) { return 1; },
        [] (auto const& cipherSuite) { return std::max(cipherSuite.GetBlockSize() - 1, std::size_t{ 1 }); },
        [] (auto const& cipherSuite) { return cipherSuite.GetBlockSize(); },
        [] (auto const& cipherSuite) { return cipherSuite.GetBlockSize() + 1; },
        [] (auto const& cipherSuite) { return (cipherSuite.GetBlockSize() * 2) + (cipherSuite.GetBlockSize() / 2); },
        [] (auto const& cipherSuite) { return 100; },
        [] (auto const& cipherSuite) { return 255; },
        [] (auto const& cipherSuite) { return 256; },
        [] (auto const& cipherSuite) { return 512; },
        [] (auto const& cipherSuite) { return 1024; },
        [] (auto const& cipherSuite) { return 4096; },
        [] (auto const& cipherSuite) { return 16384; },
        [&] (auto const& cipherSuite) { return MaximumTestInputSize; },
    };

    auto const data = Security::Test::GenerateGarbageData(MaximumTestInputSize);
    for (auto const& cipherName : Security::SupportedCipherNames) {
        SetupTestResources(test::KeyAgreementName, cipherName, test::HashFunctionName);

        for (auto const& generator : generators) {
            auto const inputSize = generator(*m_upCipherSuite);

            std::span const dataview{ data.begin(), inputSize };

            auto const optInitiatorEncrypted = m_upInitiatorPackage->Encrypt(dataview);
            ASSERT_TRUE(optInitiatorEncrypted);
            EXPECT_EQ(optInitiatorEncrypted->size(), m_upCipherSuite->GetEncryptedSize(inputSize));

            auto const optAcceptorDecrypted = m_upAcceptorPackage->Decrypt(*optInitiatorEncrypted);
            ASSERT_TRUE(optAcceptorDecrypted);
            EXPECT_TRUE(std::ranges::equal(std::span{ optAcceptorDecrypted->data(), inputSize }, dataview));

            auto const optAcceptorEncrypted = m_upAcceptorPackage->Encrypt(dataview);
            ASSERT_TRUE(optAcceptorEncrypted);
            EXPECT_EQ(optAcceptorEncrypted->size(), m_upCipherSuite->GetEncryptedSize(inputSize));

            auto const optInitiatorDecrypted = m_upInitiatorPackage->Decrypt(*optAcceptorEncrypted);
            ASSERT_TRUE(optInitiatorDecrypted);
            EXPECT_TRUE(std::ranges::equal(std::span{ optInitiatorDecrypted->data(), inputSize }, dataview));
        }
    }

    for (auto const& hashFunctionName : Security::SupportedHashFunctionNames) {
        SetupTestResources(test::KeyAgreementName, test::CipherName, hashFunctionName);

        auto data = Security::Test::GenerateGarbageData(1024);
        auto const unsignedDataSize = data.size();
        bool const success = m_upAcceptorPackage->Sign(data);
        EXPECT_TRUE(success);
        EXPECT_EQ(data.size() - unsignedDataSize, m_upCipherSuite->GetSignatureSize());
        EXPECT_EQ(m_upInitiatorPackage->Verify(data), Security::VerificationStatus::Success);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, MaliciousSignatureMutateAnyTest)
{
    constexpr auto TestDataSize = 1024;
    
    for (auto const& hashFunctionName : Security::SupportedHashFunctionNames) {
        SetupTestResources(test::KeyAgreementName, test::CipherName, hashFunctionName);

        for (auto const& probability : test::MutationProbabilities) {
            auto data = Security::Test::GenerateGarbageData(TestDataSize);

            ASSERT_TRUE(m_upAcceptorPackage->Sign(data));

            test::MutateBuffer(std::span{ data.begin(), data.size() - 1 }, probability);

            EXPECT_EQ(m_upInitiatorPackage->Verify(data), Security::VerificationStatus::Failed);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, MaliciousSignatureMutateContentTest)
{
    constexpr auto TestDataSize = 1024;
    
    for (auto const& hashFunctionName : Security::SupportedHashFunctionNames) {
        SetupTestResources(test::KeyAgreementName, test::CipherName, hashFunctionName);

        for (auto const& probability : test::MutationProbabilities) {
            auto data = Security::Test::GenerateGarbageData(TestDataSize);

            ASSERT_TRUE(m_upAcceptorPackage->Sign(data));

            test::MutateBuffer(std::span{ data.begin(), TestDataSize - 1 }, probability);

            EXPECT_EQ(m_upInitiatorPackage->Verify(data), Security::VerificationStatus::Failed);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, MaliciousSignatureMutateSignatureTest)
{
    constexpr auto TestDataSize = 1024;
    
    for (auto const& hashFunctionName : Security::SupportedHashFunctionNames) {
        SetupTestResources(test::KeyAgreementName, test::CipherName, hashFunctionName);

        for (auto const& probability : test::MutationProbabilities) {
            auto data = Security::Test::GenerateGarbageData(TestDataSize);

            ASSERT_TRUE(m_upAcceptorPackage->Sign(data));

            test::MutateBuffer(std::span{ data.begin() + TestDataSize, m_upCipherSuite->GetSignatureSize() - 1 }, probability);

            EXPECT_EQ(m_upInitiatorPackage->Verify(data), Security::VerificationStatus::Failed);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, CipherSuiteEncryptedSizeTest)
{
    using TestCases = std::map<std::string, std::map<std::size_t, std::size_t>>;
    TestCases cases = {
        {
            "aes-256-ctr",
            {
                { 0, 0 },
                { 1, 17 },
                { 2, 18 }, 
                { 100, 116 },
                { 255, 271 },
                { 256, 272 },
                { 512, 528 },
                { 1024, 1040 },
                { 4096, 4112 },
                { 16384, 16400 },
                { 65536, 65552 },
            }
        },
        {
            "aes-256-gcm",
            {
                { 0, 0 },
                { 1, 29 },
                { 2, 30 }, 
                { 100, 128 },
                { 255, 283 },
                { 256, 284 },
                { 512, 540 },
                { 1024, 1052 },
                { 4096, 4124 },
                { 16384, 16412 },
                { 65536, 65564 },
            }
        },
        {
            "camellia-256-cbc",
            {
                { 0, 0 },
                { 1, 32 },
                { 15, 32 }, 
                { 16, 48 }, 
                { 17, 48 }, 
                { 100, 128 },
                { 255, 272 },
                { 256, 288 },
                { 512, 544 },
                { 1024, 1056 },
                { 4096, 4128 },
                { 16384, 16416 },
                { 65536, 65568 },
            }
        },
        {
            "chacha20-poly1305",
            {
                { 0, 0 },
                { 1, 29 },
                { 2, 30 }, 
                { 100, 128 },
                { 255, 283 },
                { 256, 284 },
                { 512, 540 },
                { 1024, 1052 },
                { 4096, 4124 },
                { 16384, 16412 },
                { 65536, 65564 },
            }
        }
    };

    auto const data = Security::Test::GenerateGarbageData(65536);
    for (auto const& [cipherName, expectations] : cases) {
        SetupTestResources(test::KeyAgreementName, cipherName, test::HashFunctionName);
        for (auto const& [input, expected] : expectations) {
            EXPECT_EQ(m_upCipherSuite->GetEncryptedSize(input), expected);
            auto const optEncrypted = m_upInitiatorPackage->Encrypt(std::span{ data.begin(), input });
            if (expected > 0) {
                ASSERT_TRUE(optEncrypted);
                EXPECT_EQ(optEncrypted->size(), expected);
            } else {
                EXPECT_FALSE(optEncrypted);
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, InvalidKeyStoreTest)
{
    // Create the cipher suite to be used for the test run. 
    Security::CipherSuite const cipherSuite{ test::Level, test::KeyAgreementName, test::CipherName, test::HashFunctionName };

    // Create the key store that will be used to create the cipher package 
    Security::KeyStore store{ Security::PublicKey{ Security::Test::GenerateGarbageData(256) } };
    store.SetPeerPublicKey( Security::PublicKey{ Security::Test::GenerateGarbageData(256) });

    Security::CipherPackage const cipherPackage{ cipherSuite, std::move(store) };

    auto data = Security::Test::GenerateGarbageData(256);
    EXPECT_ANY_THROW(cipherPackage.Encrypt(data));
    EXPECT_ANY_THROW(cipherPackage.Decrypt(data));
    EXPECT_ANY_THROW(cipherPackage.Sign(data));
    EXPECT_ANY_THROW(cipherPackage.Verify(data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, EmptyBufferTest)
{
    for (auto const& cipherName : Security::SupportedCipherNames) {
        SetupTestResources(test::KeyAgreementName, cipherName, test::HashFunctionName);

        EXPECT_FALSE(m_upInitiatorPackage->Encrypt(Security::ReadableView{}));
        EXPECT_FALSE(m_upInitiatorPackage->Decrypt(Security::ReadableView{}));

        Security::Buffer data;
        bool const success = m_upInitiatorPackage->Sign(data);
        EXPECT_FALSE(success);
        EXPECT_TRUE(data.empty());

        EXPECT_EQ(m_upInitiatorPackage->Verify(Security::ReadableView{}), Security::VerificationStatus::Failed);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, InvalidCipherNameConstructionTest)
{
    EXPECT_ANY_THROW(
        Security::CipherSuite(
            Security::ConfidentialityLevel::Low, test::KeyAgreementName, "invalid-cipher", test::HashFunctionName)
    );
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, InvalidHashFunctionNameConstructionTest)
{
    EXPECT_ANY_THROW(
        Security::CipherSuite(
            Security::ConfidentialityLevel::Low, test::KeyAgreementName, test::CipherName, "invalid-hash-function")
    );
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, MoveConstructorTest)
{
    constexpr auto TestDataSize = 1024;

    // Create the cipher suite to be used for the test run. 
    Security::CipherSuite movedFromCipherSuite{ test::Level, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    auto const movedToCipherSuite{ std::move(movedFromCipherSuite) };

    EXPECT_EQ(movedFromCipherSuite.GetKeyAgreementName(), "");
    EXPECT_EQ(movedFromCipherSuite.GetCipherName(), "");
    EXPECT_EQ(movedFromCipherSuite.GetHashFunctionName(), "");
    EXPECT_FALSE(movedFromCipherSuite.GetCipher());
    EXPECT_FALSE(movedFromCipherSuite.GetDigest());

    EXPECT_EQ(movedToCipherSuite.GetConfidentialityLevel(), Security::ConfidentialityLevel::High);
    EXPECT_EQ(movedToCipherSuite.GetKeyAgreementName(), test::KeyAgreementName);
    EXPECT_EQ(movedToCipherSuite.GetCipherName(), test::CipherName);
    EXPECT_EQ(movedToCipherSuite.GetHashFunctionName(), test::HashFunctionName);
    EXPECT_EQ(movedToCipherSuite.GetEncryptionKeySize(), 32);
    EXPECT_EQ(movedToCipherSuite.GetInitializationVectorSize(), 16);
    EXPECT_FALSE(movedToCipherSuite.DoesCipherPadInput());
    EXPECT_FALSE(movedToCipherSuite.IsAuthenticatedCipher());
    EXPECT_TRUE(movedToCipherSuite.NeedsGeneratedInitializationVector());
    EXPECT_EQ(movedToCipherSuite.GetBlockSize(), 1);
    EXPECT_EQ(movedToCipherSuite.GetTagSize(), 0);
    EXPECT_EQ(movedToCipherSuite.GetSignatureKeySize(), 48);
    EXPECT_EQ(movedToCipherSuite.GetSignatureSize(), 48);
    
    ASSERT_TRUE(movedToCipherSuite.GetCipher());
    ASSERT_TRUE(movedToCipherSuite.GetDigest());

    // Create the key store that will be used to create the cipher package 
    auto const initiatorPublicKey = Security::PublicKey{ Security::Test::GenerateGarbageData(256) };
    auto const acceptorPublicKey = Security::PublicKey{ Security::Test::GenerateGarbageData(256) };
    auto const sharedSecret = Security::SharedSecret{ Security::Test::GenerateGarbageData(256) };

    // Create the key store that will be used to create the cipher package 
    Security::KeyStore initiatorStore{ Security::PublicKey{ initiatorPublicKey } };
    initiatorStore.SetPeerPublicKey(Security::PublicKey{ acceptorPublicKey });

    // Create the key store that will be used to create the cipher package 
    Security::KeyStore acceptorStore{ Security::PublicKey{ acceptorPublicKey } };
    acceptorStore.SetPeerPublicKey(Security::PublicKey{ initiatorPublicKey });

    auto const initialInitiatorSalt = initiatorStore.GetSalt();
    initiatorStore.PrependSessionSalt(acceptorStore.GetSalt());
    acceptorStore.AppendSessionSalt(initialInitiatorSalt);

    initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, movedToCipherSuite, sharedSecret);
    acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, movedToCipherSuite, sharedSecret);

    Security::CipherPackage movedFromInitiatorPackage{ movedToCipherSuite, std::move(initiatorStore) };
    auto const movedToInitiatorPackage{ std::move(movedFromInitiatorPackage) };

    Security::CipherPackage movedFromAcceptorPackage{ movedToCipherSuite, std::move(acceptorStore) };
    auto const movedToAcceptorPackage{ std::move(movedFromAcceptorPackage) };

    {
        auto const data = Security::Test::GenerateGarbageData(TestDataSize);
    
        EXPECT_ANY_THROW(movedFromInitiatorPackage.Encrypt(data));
        EXPECT_ANY_THROW(movedFromAcceptorPackage.Decrypt(data));
        EXPECT_ANY_THROW(movedFromAcceptorPackage.Encrypt(data));
        EXPECT_ANY_THROW(movedFromInitiatorPackage.Decrypt(data));
    }

    {
        auto const data = Security::Test::GenerateGarbageData(TestDataSize);
    
        auto const optInitiatorEncrypted = movedToInitiatorPackage.Encrypt(data);
        ASSERT_TRUE(optInitiatorEncrypted);
        EXPECT_EQ(optInitiatorEncrypted->size(), movedToCipherSuite.GetEncryptedSize(TestDataSize));

        auto const optAcceptorDecrypted = movedToAcceptorPackage.Decrypt(*optInitiatorEncrypted);
        ASSERT_TRUE(optAcceptorDecrypted);
        EXPECT_TRUE(std::ranges::equal(std::span{ optAcceptorDecrypted->data(), TestDataSize }, data));

        auto const optAcceptorEncrypted = movedToAcceptorPackage.Encrypt(data);
        ASSERT_TRUE(optAcceptorEncrypted);
        EXPECT_EQ(optAcceptorEncrypted->size(), movedToCipherSuite.GetEncryptedSize(TestDataSize));

        auto const optInitiatorDecrypted = movedToInitiatorPackage.Decrypt(*optAcceptorEncrypted);
        ASSERT_TRUE(optInitiatorDecrypted);
        EXPECT_TRUE(std::ranges::equal(std::span{ optInitiatorDecrypted->data(), TestDataSize }, data));
    }

    {
        auto data = Security::Test::GenerateGarbageData(TestDataSize);
        EXPECT_ANY_THROW(movedFromAcceptorPackage.Sign(data));
        EXPECT_ANY_THROW(movedFromInitiatorPackage.Verify(data));
    }

    {
        auto data = Security::Test::GenerateGarbageData(TestDataSize);
        auto const unsignedDataSize = data.size();
        bool const success = movedToAcceptorPackage.Sign(data);
        EXPECT_TRUE(success);
        EXPECT_EQ(data.size() - unsignedDataSize, movedToCipherSuite.GetSignatureSize());
        EXPECT_EQ(movedToInitiatorPackage.Verify(data), Security::VerificationStatus::Success);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(CipherPackageSuite, MoveAssignmentTest)
{
    constexpr auto TestDataSize = 1024;

    // Create the cipher suite to be used for the test run. 
    Security::CipherSuite movedFromCipherSuite{ test::Level, test::KeyAgreementName, test::CipherName, test::HashFunctionName };
    auto const movedToCipherSuite = std::move(movedFromCipherSuite);

    EXPECT_EQ(movedFromCipherSuite.GetKeyAgreementName(), "");
    EXPECT_EQ(movedFromCipherSuite.GetCipherName(), "");
    EXPECT_EQ(movedFromCipherSuite.GetHashFunctionName(), "");
    EXPECT_FALSE(movedFromCipherSuite.GetCipher());
    EXPECT_FALSE(movedFromCipherSuite.GetDigest());

    EXPECT_EQ(movedToCipherSuite.GetConfidentialityLevel(), Security::ConfidentialityLevel::High);
    EXPECT_EQ(movedToCipherSuite.GetKeyAgreementName(), test::KeyAgreementName);
    EXPECT_EQ(movedToCipherSuite.GetCipherName(), test::CipherName);
    EXPECT_EQ(movedToCipherSuite.GetHashFunctionName(), test::HashFunctionName);
    EXPECT_EQ(movedToCipherSuite.GetEncryptionKeySize(), 32);
    EXPECT_EQ(movedToCipherSuite.GetInitializationVectorSize(), 16);
    EXPECT_FALSE(movedToCipherSuite.DoesCipherPadInput());
    EXPECT_FALSE(movedToCipherSuite.IsAuthenticatedCipher());
    EXPECT_TRUE(movedToCipherSuite.NeedsGeneratedInitializationVector());
    EXPECT_EQ(movedToCipherSuite.GetBlockSize(), 1);
    EXPECT_EQ(movedToCipherSuite.GetTagSize(), 0);
    EXPECT_EQ(movedToCipherSuite.GetSignatureKeySize(), 48);
    EXPECT_EQ(movedToCipherSuite.GetSignatureSize(), 48);
    
    ASSERT_TRUE(movedToCipherSuite.GetCipher());
    ASSERT_TRUE(movedToCipherSuite.GetDigest());

    // Create the key store that will be used to create the cipher package 
    auto const initiatorPublicKey = Security::PublicKey{ Security::Test::GenerateGarbageData(256) };
    auto const acceptorPublicKey = Security::PublicKey{ Security::Test::GenerateGarbageData(256) };
    auto const sharedSecret = Security::SharedSecret{ Security::Test::GenerateGarbageData(256) };

    // Create the key store that will be used to create the cipher package 
    Security::KeyStore initiatorStore{ Security::PublicKey{ initiatorPublicKey } };
    initiatorStore.SetPeerPublicKey(Security::PublicKey{ acceptorPublicKey });

    // Create the key store that will be used to create the cipher package 
    Security::KeyStore acceptorStore{ Security::PublicKey{ acceptorPublicKey } };
    acceptorStore.SetPeerPublicKey(Security::PublicKey{ initiatorPublicKey });

    auto const initialInitiatorSalt = initiatorStore.GetSalt();
    initiatorStore.PrependSessionSalt(acceptorStore.GetSalt());
    acceptorStore.AppendSessionSalt(initialInitiatorSalt);

    initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, movedToCipherSuite, sharedSecret);
    acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, movedToCipherSuite, sharedSecret);

    Security::CipherPackage movedFromInitiatorPackage{ movedToCipherSuite, std::move(initiatorStore) };
    auto const movedToInitiatorPackage = std::move(movedFromInitiatorPackage);

    Security::CipherPackage movedFromAcceptorPackage{ movedToCipherSuite, std::move(acceptorStore) };
    auto const movedToAcceptorPackage = std::move(movedFromAcceptorPackage);

    {
        auto const data = Security::Test::GenerateGarbageData(TestDataSize);
    
        EXPECT_ANY_THROW(movedFromInitiatorPackage.Encrypt(data));
        EXPECT_ANY_THROW(movedFromAcceptorPackage.Decrypt(data));
        EXPECT_ANY_THROW(movedFromAcceptorPackage.Encrypt(data));
        EXPECT_ANY_THROW(movedFromInitiatorPackage.Decrypt(data));
    }

    {
        auto const data = Security::Test::GenerateGarbageData(TestDataSize);
    
        auto const optInitiatorEncrypted = movedToInitiatorPackage.Encrypt(data);
        ASSERT_TRUE(optInitiatorEncrypted);
        EXPECT_EQ(optInitiatorEncrypted->size(), movedToCipherSuite.GetEncryptedSize(TestDataSize));

        auto const optAcceptorDecrypted = movedToAcceptorPackage.Decrypt(*optInitiatorEncrypted);
        ASSERT_TRUE(optAcceptorDecrypted);
        EXPECT_TRUE(std::ranges::equal(std::span{ optAcceptorDecrypted->data(), TestDataSize }, data));

        auto const optAcceptorEncrypted = movedToAcceptorPackage.Encrypt(data);
        ASSERT_TRUE(optAcceptorEncrypted);
        EXPECT_EQ(optAcceptorEncrypted->size(), movedToCipherSuite.GetEncryptedSize(TestDataSize));

        auto const optInitiatorDecrypted = movedToInitiatorPackage.Decrypt(*optAcceptorEncrypted);
        ASSERT_TRUE(optInitiatorDecrypted);
        EXPECT_TRUE(std::ranges::equal(std::span{ optInitiatorDecrypted->data(), TestDataSize }, data));
    }

    {
        auto data = Security::Test::GenerateGarbageData(TestDataSize);

        Security::Buffer signature;
        EXPECT_ANY_THROW(movedFromInitiatorPackage.Sign(data, signature));
        EXPECT_ANY_THROW(movedFromAcceptorPackage.Verify(data));
        EXPECT_ANY_THROW(movedFromAcceptorPackage.Sign(data));
        EXPECT_ANY_THROW(movedFromInitiatorPackage.Verify(data));
    }

    {
        auto data = Security::Test::GenerateGarbageData(TestDataSize);
        auto const unsignedDataSize = data.size();
        bool const success = movedToAcceptorPackage.Sign(data);
        EXPECT_TRUE(success);
        EXPECT_EQ(data.size() - unsignedDataSize, movedToCipherSuite.GetSignatureSize());
        EXPECT_EQ(movedToInitiatorPackage.Verify(data), Security::VerificationStatus::Success);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void test::MutateBuffer(Security::WriteableView buffer, double probability)
{
    std::random_device device;
    std::mt19937 generator(device());
    std::uniform_int_distribution<std::uint16_t> byteDistribution(std::numeric_limits<std::uint8_t>::min(), std::numeric_limits<std::uint8_t>::max());
    std::bernoulli_distribution shouldMutate(probability);

    std::ranges::for_each(buffer, [&] (std::uint8_t& byte) {
        if (shouldMutate(generator)) {
            byte = byteDistribution(generator); 
        }
    });

    // Ensure at least one byte has been mutated. 
    std::uniform_int_distribution<std::uint16_t> positionDistribution(0, buffer.size() - 1);
    auto const position = positionDistribution(generator);
    for (auto const original = buffer[position]; buffer[position] == original;) {
        buffer[position] = byteDistribution(generator); 
    }
}

//----------------------------------------------------------------------------------------------------------------------
