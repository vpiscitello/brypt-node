//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Security/CipherPackage.hpp"
#include "Components/Security/KeyStore.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
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

static inline std::string const KeyAgreementName = "kem-kyber768";
static inline std::string const CipherName = "aes-256-ctr";
static inline std::string const HashFunctionName = "sha384";

static inline Security::CipherSuite const CipherSuite{ 
    Security::ConfidentialityLevel::High,
    test::KeyAgreementName,
    test::CipherName,
    test::HashFunctionName
};

constexpr std::size_t ExpectedEncryptionKeySize = 32;
constexpr std::size_t ExpectedSignatureKeySize = 48;

static inline Security::PublicKey PublicKey{ Security::Test::GenerateGarbageData(65536) };
static inline Security::PublicKey PeerPublicKey{ Security::Test::GenerateGarbageData(65536) };
static inline Security::Salt Salt{ Security::Test::GenerateGarbageData(Security::KeyStore::PrincipalRandomSize) };
static inline Security::SharedSecret SharedSecret{ Security::Test::GenerateGarbageData(96) };

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, PublicKeyConstructorTest)
{
    Security::KeyStore const store{ Security::PublicKey{ test::PublicKey } };
    EXPECT_EQ(store.GetPublicKey(), test::PublicKey);
    EXPECT_EQ(store.GetPublicKeySize(), test::PublicKey.GetSize());
    EXPECT_FALSE(store.GetPeerPublicKey());
    EXPECT_FALSE(store.GetSalt().IsEmpty());
    EXPECT_EQ(store.GetSaltSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(store.HasGeneratedKeys());
    EXPECT_FALSE(store.GetContentKey());
    EXPECT_FALSE(store.GetPeerContentKey());
    EXPECT_FALSE(store.GetSignatureKey());
    EXPECT_FALSE(store.GetPeerSignatureKey());
    EXPECT_EQ(store.GetVerificationDataSize(), Security::KeyStore::PrincipalRandomSize);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, EmptyPublicKeyConstructorTest)
{
    EXPECT_ANY_THROW(Security::KeyStore{ Security::PublicKey{} });
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, DefaultMoveConstructorTest)
{
    Security::KeyStore firstStore{ Security::PublicKey{ test::PublicKey } };
    Security::KeyStore const secondStore{ std::move(firstStore) };

    EXPECT_TRUE(firstStore.GetPublicKey().IsEmpty());
    EXPECT_FALSE(firstStore.GetPeerPublicKey());
    EXPECT_TRUE(firstStore.GetSalt().IsEmpty());
    EXPECT_EQ(firstStore.GetSaltSize(), std::size_t{ 0 });
    EXPECT_FALSE(firstStore.GetContentKey());
    EXPECT_FALSE(firstStore.GetPeerContentKey());
    EXPECT_FALSE(firstStore.GetSignatureKey());
    EXPECT_FALSE(firstStore.GetPeerSignatureKey());
    EXPECT_EQ(firstStore.GetVerificationDataSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(firstStore.HasGeneratedKeys());

    EXPECT_EQ(secondStore.GetPublicKey(), test::PublicKey);
    EXPECT_EQ(secondStore.GetPublicKeySize(), test::PublicKey.GetSize());
    EXPECT_FALSE(secondStore.GetPeerPublicKey());
    EXPECT_FALSE(secondStore.GetSalt().IsEmpty());
    EXPECT_EQ(secondStore.GetSaltSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(secondStore.HasGeneratedKeys());
    EXPECT_FALSE(secondStore.GetContentKey());
    EXPECT_FALSE(secondStore.GetPeerContentKey());
    EXPECT_FALSE(secondStore.GetSignatureKey());
    EXPECT_FALSE(secondStore.GetPeerSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, DefaultMoveAssignmentOperatorTest)
{
    Security::KeyStore firstStore{ Security::PublicKey{ test::PublicKey } };
    Security::KeyStore const secondStore = std::move(firstStore);

    EXPECT_TRUE(firstStore.GetPublicKey().IsEmpty());
    EXPECT_FALSE(firstStore.GetPeerPublicKey());
    EXPECT_TRUE(firstStore.GetSalt().IsEmpty());
    EXPECT_EQ(firstStore.GetSaltSize(), std::size_t{ 0 });
    EXPECT_FALSE(firstStore.HasGeneratedKeys());
    EXPECT_FALSE(firstStore.GetContentKey());
    EXPECT_FALSE(firstStore.GetPeerContentKey());
    EXPECT_FALSE(firstStore.GetSignatureKey());
    EXPECT_FALSE(firstStore.GetPeerSignatureKey());
    EXPECT_EQ(firstStore.GetVerificationDataSize(), Security::KeyStore::PrincipalRandomSize);

    EXPECT_EQ(secondStore.GetPublicKey(), test::PublicKey);
    EXPECT_EQ(secondStore.GetPublicKeySize(), test::PublicKey.GetSize());
    EXPECT_FALSE(secondStore.GetPeerPublicKey());
    EXPECT_FALSE(secondStore.GetSalt().IsEmpty());
    EXPECT_EQ(secondStore.GetSaltSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(secondStore.HasGeneratedKeys());
    EXPECT_FALSE(secondStore.GetContentKey());
    EXPECT_FALSE(secondStore.GetPeerContentKey());
    EXPECT_FALSE(secondStore.GetSignatureKey());
    EXPECT_FALSE(secondStore.GetPeerSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, SetPeerPublicKeyTest)
{
    Security::KeyStore store{ Security::PublicKey{ test::PublicKey } };
    store.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });

    EXPECT_EQ(store.GetPublicKey(), test::PublicKey);
    EXPECT_EQ(store.GetPublicKeySize(), test::PublicKey.GetSize());

    {
        auto const& optPeerPublicKey = store.GetPeerPublicKey();
        ASSERT_TRUE(optPeerPublicKey);
        EXPECT_EQ(*optPeerPublicKey, test::PeerPublicKey);
    }
    
    EXPECT_FALSE(store.HasGeneratedKeys());
    EXPECT_FALSE(store.GetSalt().IsEmpty());
    EXPECT_EQ(store.GetSaltSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(store.GetContentKey());
    EXPECT_FALSE(store.GetPeerContentKey());
    EXPECT_FALSE(store.GetSignatureKey());
    EXPECT_FALSE(store.GetPeerSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, AppendSaltTest)
{
    Security::KeyStore store{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultSalt = store.GetSalt();
    EXPECT_FALSE(store.GetSalt().IsEmpty());
    EXPECT_EQ(store.GetSaltSize(), Security::KeyStore::PrincipalRandomSize);

    store.AppendSessionSalt(test::Salt);
    EXPECT_FALSE(store.GetSalt().IsEmpty());
    EXPECT_EQ(store.GetSaltSize(), Security::KeyStore::PrincipalRandomSize * 2);

    auto const data = store.GetSalt().GetData();
    auto const firstSaltPartition = std::span{ data.begin(), data.begin() + Security::KeyStore::PrincipalRandomSize };
    EXPECT_TRUE(std::ranges::equal(firstSaltPartition, defaultSalt.GetData()));

    auto const secondSaltPartition = std::span{ data.begin() + Security::KeyStore::PrincipalRandomSize, data.end() };
    EXPECT_TRUE(std::ranges::equal(secondSaltPartition, test::Salt.GetData()));

    EXPECT_FALSE(store.HasGeneratedKeys());
    EXPECT_FALSE(store.GetContentKey());
    EXPECT_FALSE(store.GetPeerContentKey());
    EXPECT_FALSE(store.GetSignatureKey());
    EXPECT_FALSE(store.GetPeerSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, PrependSaltTest)
{
    Security::KeyStore store{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultSalt = store.GetSalt();
    EXPECT_FALSE(store.GetSalt().IsEmpty());
    EXPECT_EQ(store.GetSaltSize(), Security::KeyStore::PrincipalRandomSize);

    store.PrependSessionSalt(test::Salt);
    EXPECT_FALSE(store.GetSalt().IsEmpty());
    EXPECT_EQ(store.GetSaltSize(), Security::KeyStore::PrincipalRandomSize * 2);

    auto const data = store.GetSalt().GetData();
    auto const firstSaltPartition = std::span{ data.begin(), data.begin() + Security::KeyStore::PrincipalRandomSize };
    EXPECT_TRUE(std::ranges::equal(firstSaltPartition, test::Salt.GetData()));

    auto const secondSaltPartition = std::span{ data.begin() + Security::KeyStore::PrincipalRandomSize, data.end() };
    EXPECT_TRUE(std::ranges::equal(secondSaltPartition, defaultSalt.GetData()));

    EXPECT_FALSE(store.HasGeneratedKeys());
    EXPECT_FALSE(store.GetContentKey());
    EXPECT_FALSE(store.GetPeerContentKey());
    EXPECT_FALSE(store.GetSignatureKey());
    EXPECT_FALSE(store.GetPeerSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysInitatorTest)
{
    Security::KeyStore store{ Security::PublicKey{ test::PublicKey } };
    store.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    store.PrependSessionSalt(test::Salt);

    auto const optVerificationData = store.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
    ASSERT_TRUE(optVerificationData);
    EXPECT_EQ(optVerificationData->GetSize(), store.GetVerificationDataSize());
    EXPECT_TRUE(store.HasGeneratedKeys());
    
    auto const& optContentKey = store.GetContentKey();
    ASSERT_TRUE(optContentKey);
    EXPECT_FALSE(optContentKey->IsEmpty());
    EXPECT_EQ(optContentKey->GetSize(), test::ExpectedEncryptionKeySize);

    auto const& optPeerContentKey = store.GetPeerContentKey();
    ASSERT_TRUE(optPeerContentKey);
    EXPECT_FALSE(optPeerContentKey->IsEmpty());
    EXPECT_EQ(optPeerContentKey->GetSize(), test::ExpectedEncryptionKeySize);
    EXPECT_NE(*optContentKey, *optPeerContentKey);

    auto const& optSignatureKey = store.GetSignatureKey();
    ASSERT_TRUE(optSignatureKey);
    EXPECT_FALSE(optSignatureKey->IsEmpty());
    EXPECT_EQ(optSignatureKey->GetSize(), test::ExpectedSignatureKeySize);
    EXPECT_NE(optContentKey->GetData(), optSignatureKey->GetData());

    auto const& optPeerSignatureKey = store.GetPeerSignatureKey();
    ASSERT_TRUE(optPeerSignatureKey);
    EXPECT_FALSE(optPeerSignatureKey->IsEmpty());
    EXPECT_EQ(optPeerSignatureKey->GetSize(), test::ExpectedSignatureKeySize);
    EXPECT_NE(*optSignatureKey, *optPeerSignatureKey);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysAcceptorTest)
{
    Security::KeyStore store{ Security::PublicKey{ test::PublicKey } };
    store.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    store.PrependSessionSalt(test::Salt);

    auto const optVerificationData = store.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
    ASSERT_TRUE(optVerificationData);
    EXPECT_EQ(optVerificationData->GetSize(), store.GetVerificationDataSize());
    EXPECT_TRUE(store.HasGeneratedKeys());
    
    auto const& optContentKey = store.GetContentKey();
    ASSERT_TRUE(optContentKey);
    EXPECT_FALSE(optContentKey->IsEmpty());
    EXPECT_EQ(optContentKey->GetSize(), test::ExpectedEncryptionKeySize);

    auto const& optPeerContentKey = store.GetPeerContentKey();
    ASSERT_TRUE(optPeerContentKey);
    EXPECT_FALSE(optPeerContentKey->IsEmpty());
    EXPECT_EQ(optPeerContentKey->GetSize(), test::ExpectedEncryptionKeySize);
    EXPECT_NE(*optContentKey, *optPeerContentKey);

    auto const& optSignatureKey = store.GetSignatureKey();
    ASSERT_TRUE(optSignatureKey);
    EXPECT_FALSE(optSignatureKey->IsEmpty());
    EXPECT_EQ(optSignatureKey->GetSize(), test::ExpectedSignatureKeySize);
    EXPECT_NE(optContentKey->GetData(), optSignatureKey->GetData());

    auto const& optPeerSignatureKey = store.GetPeerSignatureKey();
    ASSERT_TRUE(optPeerSignatureKey);
    EXPECT_FALSE(optPeerSignatureKey->IsEmpty());
    EXPECT_EQ(optPeerSignatureKey->GetSize(), test::ExpectedSignatureKeySize);
    EXPECT_NE(*optSignatureKey, *optPeerSignatureKey);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysSameSharedSecretTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultInitiatorSalt = initiatorStore.GetSalt();

    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultAcceptorSalt = acceptorStore.GetSalt();

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(defaultAcceptorSalt);

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(defaultInitiatorSalt);

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_EQ(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_EQ(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_EQ(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_EQ(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_EQ(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysUnsharedSaltTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_NE(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_NE(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_NE(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_NE(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_NE(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysUnknownInitatorSaltTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultAcceptorSalt = acceptorStore.GetSalt();

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(defaultAcceptorSalt);

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(Security::Salt{ Security::Test::GenerateGarbageData(Security::KeyStore::PrincipalRandomSize) });

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_NE(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_NE(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_NE(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_NE(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_NE(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysUnknownAcceptorSaltTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultInitiatorSalt = initiatorStore.GetSalt();

    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(Security::Salt{ Security::Test::GenerateGarbageData(Security::KeyStore::PrincipalRandomSize) });

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(defaultInitiatorSalt);

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_NE(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_NE(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_NE(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_NE(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_NE(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysRandomSaltTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(Security::Salt{ Security::Test::GenerateGarbageData(Security::KeyStore::PrincipalRandomSize) });

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(Security::Salt{ Security::Test::GenerateGarbageData(Security::KeyStore::PrincipalRandomSize) });

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_NE(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_NE(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_NE(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_NE(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_NE(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysMutatedIniatorSaltTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt defaultInitiatorSalt = initiatorStore.GetSalt();
    defaultInitiatorSalt.GetData()[defaultInitiatorSalt.GetSize() / 2] = defaultInitiatorSalt.GetData()[defaultInitiatorSalt.GetSize() / 2] - 1;

    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultAcceptorSalt = acceptorStore.GetSalt();

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(defaultAcceptorSalt);

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(defaultInitiatorSalt);

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_NE(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_NE(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_NE(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_NE(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_NE(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GenerateSessionKeysMutatedAcceptorSaltTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultInitiatorSalt = initiatorStore.GetSalt();

    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt defaultAcceptorSalt = acceptorStore.GetSalt();
    defaultAcceptorSalt.GetData()[defaultAcceptorSalt.GetSize() / 2] = defaultAcceptorSalt.GetData()[defaultAcceptorSalt.GetSize() / 2] - 1;

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(defaultAcceptorSalt);

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(defaultInitiatorSalt);

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_NE(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_NE(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_NE(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_NE(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_NE(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GeneratedMoveConstructorTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultInitiatorSalt = initiatorStore.GetSalt();

    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultAcceptorSalt = acceptorStore.GetSalt();

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(defaultAcceptorSalt);

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(defaultInitiatorSalt);

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_EQ(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_EQ(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_EQ(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_EQ(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_EQ(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());

    auto const movedInitiatorStore{ std::move(initiatorStore) };

    EXPECT_TRUE(initiatorStore.GetPublicKey().IsEmpty());
    EXPECT_FALSE(initiatorStore.GetPeerPublicKey());
    EXPECT_TRUE(initiatorStore.GetSalt().IsEmpty());
    EXPECT_EQ(initiatorStore.GetSaltSize(), std::size_t{ 0 });
    EXPECT_FALSE(initiatorStore.GetContentKey());
    EXPECT_FALSE(initiatorStore.GetPeerContentKey());
    EXPECT_FALSE(initiatorStore.GetSignatureKey());
    EXPECT_FALSE(initiatorStore.GetPeerSignatureKey());
    EXPECT_EQ(initiatorStore.GetVerificationDataSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(initiatorStore.HasGeneratedKeys());

    EXPECT_NE(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_NE(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_NE(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_NE(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());

    EXPECT_EQ(movedInitiatorStore.GetPublicKey(), test::PublicKey);
    EXPECT_EQ(movedInitiatorStore.GetPublicKeySize(), test::PublicKey.GetSize());
    EXPECT_TRUE(movedInitiatorStore.GetPeerPublicKey());
    EXPECT_FALSE(movedInitiatorStore.GetSalt().IsEmpty());
    EXPECT_EQ(movedInitiatorStore.GetSaltSize(), Security::KeyStore::PrincipalRandomSize * 2);
    EXPECT_TRUE(movedInitiatorStore.HasGeneratedKeys());
    EXPECT_EQ(movedInitiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_EQ(movedInitiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_EQ(movedInitiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_EQ(movedInitiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GeneratedMoveAssignmentOperatorTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultInitiatorSalt = initiatorStore.GetSalt();

    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultAcceptorSalt = acceptorStore.GetSalt();

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(defaultAcceptorSalt);

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(defaultInitiatorSalt);

    {
        auto const optInitiatorVerificationData = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optInitiatorVerificationData);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());

        auto const optAcceptorVerificationData = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        ASSERT_TRUE(optAcceptorVerificationData);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());

        EXPECT_EQ(optInitiatorVerificationData, optAcceptorVerificationData);
    }

    EXPECT_EQ(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_EQ(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_EQ(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_EQ(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());

    auto const movedInitiatorStore =  std::move(initiatorStore);

    EXPECT_TRUE(initiatorStore.GetPublicKey().IsEmpty());
    EXPECT_FALSE(initiatorStore.GetPeerPublicKey());
    EXPECT_TRUE(initiatorStore.GetSalt().IsEmpty());
    EXPECT_EQ(initiatorStore.GetSaltSize(), std::size_t{ 0 });
    EXPECT_FALSE(initiatorStore.GetContentKey());
    EXPECT_FALSE(initiatorStore.GetPeerContentKey());
    EXPECT_FALSE(initiatorStore.GetSignatureKey());
    EXPECT_FALSE(initiatorStore.GetPeerSignatureKey());
    EXPECT_EQ(initiatorStore.GetVerificationDataSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(initiatorStore.HasGeneratedKeys());

    EXPECT_NE(initiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_NE(initiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_NE(initiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_NE(initiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());

    EXPECT_EQ(movedInitiatorStore.GetPublicKey(), test::PublicKey);
    EXPECT_EQ(movedInitiatorStore.GetPublicKeySize(), test::PublicKey.GetSize());
    EXPECT_TRUE(movedInitiatorStore.GetPeerPublicKey());
    EXPECT_FALSE(movedInitiatorStore.GetSalt().IsEmpty());
    EXPECT_EQ(movedInitiatorStore.GetSaltSize(), Security::KeyStore::PrincipalRandomSize * 2);
    EXPECT_TRUE(movedInitiatorStore.HasGeneratedKeys());
    EXPECT_EQ(movedInitiatorStore.GetContentKey(), acceptorStore.GetPeerContentKey());
    EXPECT_EQ(movedInitiatorStore.GetPeerContentKey(), acceptorStore.GetContentKey());
    EXPECT_EQ(movedInitiatorStore.GetSignatureKey(), acceptorStore.GetPeerSignatureKey());
    EXPECT_EQ(movedInitiatorStore.GetPeerSignatureKey(), acceptorStore.GetSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, SmallSharedSecretTest)
{
    Security::KeyStore store{ Security::PublicKey{ test::PublicKey } };
    EXPECT_FALSE(store.GenerateSessionKeys(
        Security::ExchangeRole::Initiator, test::CipherSuite, Security::SharedSecret{ Security::Test::GenerateGarbageData(15) }));
    
    EXPECT_FALSE(store.HasGeneratedKeys());
    EXPECT_FALSE(store.GetContentKey());
    EXPECT_FALSE(store.GetPeerContentKey());
    EXPECT_FALSE(store.GetSignatureKey());
    EXPECT_FALSE(store.GetPeerSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, LargeSharedSecretTest)
{
    Security::KeyStore store{ Security::PublicKey{ test::PublicKey } };
    EXPECT_FALSE(store.GenerateSessionKeys(
        Security::ExchangeRole::Initiator, test::CipherSuite, Security::SharedSecret{ Security::Test::GenerateGarbageData(1025) }));
    
    EXPECT_FALSE(store.HasGeneratedKeys());
    EXPECT_FALSE(store.GetContentKey());
    EXPECT_FALSE(store.GetPeerContentKey());
    EXPECT_FALSE(store.GetSignatureKey());
    EXPECT_FALSE(store.GetPeerSignatureKey());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, DefaultEraseTest)
{
    Security::KeyStore store{ Security::PublicKey{ test::PublicKey } };
    store.Erase();

    EXPECT_TRUE(store.GetPublicKey().IsEmpty());
    EXPECT_FALSE(store.GetPeerPublicKey());
    EXPECT_TRUE(store.GetSalt().IsEmpty());
    EXPECT_EQ(store.GetSaltSize(), std::size_t{ 0 });
    EXPECT_FALSE(store.GetContentKey());
    EXPECT_FALSE(store.GetPeerContentKey());
    EXPECT_FALSE(store.GetSignatureKey());
    EXPECT_FALSE(store.GetPeerSignatureKey());
    EXPECT_EQ(store.GetVerificationDataSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(store.HasGeneratedKeys());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(KeyStoreSuite, GeneratedEraseTest)
{
    Security::KeyStore initiatorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultInitiatorSalt = initiatorStore.GetSalt();

    Security::KeyStore acceptorStore{ Security::PublicKey{ test::PublicKey } };
    Security::Salt const defaultAcceptorSalt = acceptorStore.GetSalt();

    initiatorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    initiatorStore.PrependSessionSalt(defaultAcceptorSalt);

    acceptorStore.SetPeerPublicKey(Security::PublicKey{ test::PeerPublicKey });
    acceptorStore.AppendSessionSalt(defaultInitiatorSalt);

    {
        [[maybe_unused]] auto const result = initiatorStore.GenerateSessionKeys(Security::ExchangeRole::Initiator, test::CipherSuite, test::SharedSecret);
        EXPECT_TRUE(initiatorStore.HasGeneratedKeys());
    }

    {
        [[maybe_unused]] auto const result = acceptorStore.GenerateSessionKeys(Security::ExchangeRole::Acceptor, test::CipherSuite, test::SharedSecret);
        EXPECT_TRUE(acceptorStore.HasGeneratedKeys());
    }

    acceptorStore.Erase();

    EXPECT_TRUE(acceptorStore.GetPublicKey().IsEmpty());
    EXPECT_FALSE(acceptorStore.GetPeerPublicKey());
    EXPECT_TRUE(acceptorStore.GetSalt().IsEmpty());
    EXPECT_EQ(acceptorStore.GetSaltSize(), std::size_t{ 0 });
    EXPECT_FALSE(acceptorStore.GetContentKey());
    EXPECT_FALSE(acceptorStore.GetPeerContentKey());
    EXPECT_FALSE(acceptorStore.GetSignatureKey());
    EXPECT_FALSE(acceptorStore.GetPeerSignatureKey());
    EXPECT_EQ(acceptorStore.GetVerificationDataSize(), Security::KeyStore::PrincipalRandomSize);
    EXPECT_FALSE(acceptorStore.HasGeneratedKeys());
}

//----------------------------------------------------------------------------------------------------------------------
