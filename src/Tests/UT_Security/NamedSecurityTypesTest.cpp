//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
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

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(PublicKeySuite, DefaultConstructorTest)
{
    Security::PublicKey key;
    EXPECT_TRUE(key.IsEmpty());
    EXPECT_EQ(key.GetSize(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PublicKeySuite, BufferMoveConstructorTest)
{
    auto data = Security::Test::GenerateGarbageData(65536);
    Security::PublicKey const key{ std::move(data) };
    EXPECT_FALSE(key.IsEmpty());
    EXPECT_GT(key.GetSize(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PublicKeySuite, BufferCopyConstructorTest)
{
    Security::PublicKey const firstKey{ Security::Test::GenerateGarbageData(65536) };
    Security::PublicKey const secondKey{ firstKey };
    EXPECT_EQ(firstKey.GetSize(), secondKey.GetSize());
    EXPECT_TRUE(std::ranges::equal(firstKey.GetData(), secondKey.GetData()));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PublicKeySuite, MoveAssignmentOperatorTest) 
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::PublicKey firstKey{ Security::Buffer{ data } };
    Security::PublicKey const secondKey = std::move(firstKey);

    EXPECT_TRUE(firstKey.IsEmpty());
    EXPECT_FALSE(secondKey.IsEmpty());
    EXPECT_EQ(secondKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(secondKey.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PublicKeySuite, ReadTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::PublicKey const key{ Security::Buffer{ data } };
    auto const result = key.Read<std::size_t>([] (Security::Buffer const& buffer) {
        return buffer.size();
    });
    EXPECT_EQ(result, data.size());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PublicKeySuite, EraseTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::PublicKey key{ Security::Buffer{ data } };

    EXPECT_EQ(key.GetSize(), data.size());
    key.Erase();
    EXPECT_TRUE(key.IsEmpty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SaltSuite, DefaultConstructorTest)
{
    Security::Salt salt;
    EXPECT_TRUE(salt.IsEmpty());
    EXPECT_EQ(salt.GetSize(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SaltSuite, MoveConstructorTest)
{
    auto data = Security::Test::GenerateGarbageData(65536);
    Security::Salt const salt{ std::move(data) };
    EXPECT_FALSE(salt.IsEmpty());
    EXPECT_GT(salt.GetSize(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SaltSuite, CopyConstructorTest)
{
    Security::Salt const firstSalt{ Security::Test::GenerateGarbageData(65536) };
    Security::Salt const secondSalt{ firstSalt };
    EXPECT_EQ(firstSalt.GetSize(), secondSalt.GetSize());
    EXPECT_TRUE(std::ranges::equal(firstSalt.GetData(), secondSalt.GetData()));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SaltSuite, MoveAssignmentOperatorTest) 
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::Salt firstSalt{ Security::Buffer{ data } };
    Security::Salt const secondSalt = std::move(firstSalt);

    EXPECT_TRUE(firstSalt.IsEmpty());
    EXPECT_FALSE(secondSalt.IsEmpty());
    EXPECT_EQ(secondSalt.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(secondSalt.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SaltSuite, AppendTest)
{
    // TODO: Append
    constexpr std::size_t TotalSize = 65536;
    constexpr std::size_t Midpoint = TotalSize / 2;
    auto const data = Security::Test::GenerateGarbageData(TotalSize);
    auto const firstPartition = std::span{ data.begin(), data.begin() + Midpoint };
    auto const secondPartition = std::span{ data.begin() + Midpoint, data.end() };
    
    Security::Salt salt;
    EXPECT_TRUE(salt.IsEmpty());

    salt.Append(firstPartition);
    EXPECT_EQ(salt.GetSize(), firstPartition.size());

    salt.Append(secondPartition);
    EXPECT_EQ(salt.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(salt.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SaltSuite, EraseTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::Salt salt{ Security::Buffer{ data } };

    EXPECT_EQ(salt.GetSize(), data.size());
    salt.Erase();
    EXPECT_TRUE(salt.IsEmpty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SharedSecretSuite, BufferConstructorTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SharedSecret const sharedSecret{ Security::Buffer{ data } };
    EXPECT_EQ(sharedSecret.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(sharedSecret.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SharedSecretSuite, MoveConstructorTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SharedSecret firstSharedSecret{ Security::Buffer{ data } };
    Security::SharedSecret const secondSharedSecret{ std::move(firstSharedSecret) };
    EXPECT_TRUE(firstSharedSecret.IsEmpty());
    EXPECT_FALSE(secondSharedSecret.IsEmpty());
    EXPECT_EQ(secondSharedSecret.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(secondSharedSecret.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SharedSecretSuite, MoveAssignmentOperatorTest) 
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SharedSecret firstSharedSecret{ Security::Buffer{ data } };
    Security::SharedSecret const secondSharedSecret = std::move(firstSharedSecret);
    EXPECT_TRUE(firstSharedSecret.IsEmpty());
    EXPECT_FALSE(secondSharedSecret.IsEmpty());
    EXPECT_EQ(secondSharedSecret.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(secondSharedSecret.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SupplementalDataSuite, DefaultConstructorTest)
{
    Security::SupplementalData supplementalData;
    EXPECT_TRUE(supplementalData.IsEmpty());
    EXPECT_EQ(supplementalData.GetSize(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SupplementalDataSuite, MoveConstructorTest)
{
    auto data = Security::Test::GenerateGarbageData(65536);
    Security::SupplementalData const supplementalData{ std::move(data) };
    EXPECT_FALSE(supplementalData.IsEmpty());
    EXPECT_GT(supplementalData.GetSize(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SupplementalDataSuite, MoveAssignmentOperatorTest) 
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SupplementalData firstSupplementalData{ Security::Buffer{ data } };
    Security::SupplementalData const secondSupplementalData = std::move(firstSupplementalData);

    EXPECT_TRUE(firstSupplementalData.IsEmpty());
    EXPECT_FALSE(secondSupplementalData.IsEmpty());
    EXPECT_EQ(secondSupplementalData.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(secondSupplementalData.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SupplementalDataSuite, ReadTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SupplementalData const supplementalData{ Security::Buffer{ data } };
    auto const result = supplementalData.Read<std::size_t>([] (Security::Buffer const& buffer) {
        return buffer.size();
    });
    EXPECT_EQ(result, data.size());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PrincipalKeySuite, BufferMoveConstructorTest)
{
    auto data = Security::Test::GenerateGarbageData(65536);
    Security::PrincipalKey const principalKey{ std::move(data) };
    EXPECT_FALSE(principalKey.IsEmpty());
    EXPECT_GT(principalKey.GetSize(), 0);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PrincipalKeySuite, MoveConstructorTest) 
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::PrincipalKey firstPrincipalKey{ Security::Buffer{ data } };
    Security::PrincipalKey const secondPrincipalKey{ std::move(firstPrincipalKey) };

    EXPECT_TRUE(firstPrincipalKey.IsEmpty());
    EXPECT_FALSE(secondPrincipalKey.IsEmpty());
    EXPECT_EQ(secondPrincipalKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(secondPrincipalKey.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PrincipalKeySuite, MoveAssignmentOperatorTest) 
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::PrincipalKey firstPrincipalKey{ Security::Buffer{ data } };
    Security::PrincipalKey const secondPrincipalKey = std::move(firstPrincipalKey);

    EXPECT_TRUE(firstPrincipalKey.IsEmpty());
    EXPECT_FALSE(secondPrincipalKey.IsEmpty());
    EXPECT_EQ(secondPrincipalKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(secondPrincipalKey.GetData(), data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PrincipalKeySuite, GetCordonsTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::PrincipalKey const principalKey{ Security::Buffer{ data } };

    constexpr std::size_t start = 8192;
    constexpr std::size_t end = 10240;
    auto const cordon = principalKey.GetCordon(start, end - start);

    EXPECT_EQ(cordon.size(), end - start);
    EXPECT_TRUE(std::ranges::equal(cordon, std::span{ data.begin() + start, data.begin() + end }));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PrincipalKeySuite, EraseTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::PrincipalKey principalKey{ Security::Buffer{ data } };

    EXPECT_EQ(principalKey.GetSize(), data.size());
    principalKey.Erase();
    EXPECT_TRUE(principalKey.IsEmpty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EncryptionKeySuite, ViewConstructorTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::EncryptionKey const encryptionKey{ data };
    EXPECT_FALSE(encryptionKey.IsEmpty());
    EXPECT_EQ(encryptionKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(std::span{ encryptionKey.GetData(), encryptionKey.GetSize() }, data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EncryptionKeySuite, MoveConstructorTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::EncryptionKey firstEncryptionKey{ data };
    Security::EncryptionKey const secondEncryptionKey{ std::move(firstEncryptionKey) };
    EXPECT_TRUE(firstEncryptionKey.IsEmpty());
    EXPECT_EQ(secondEncryptionKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(std::span{ secondEncryptionKey.GetData(), secondEncryptionKey.GetSize() }, data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EncryptionKeySuite, MoveAssignmentOperatorTest) 
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::EncryptionKey firstEncryptionKey{ data };
    Security::EncryptionKey const secondEncryptionKey = std::move(firstEncryptionKey);
    EXPECT_TRUE(firstEncryptionKey.IsEmpty());
    EXPECT_EQ(secondEncryptionKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(std::span{ secondEncryptionKey.GetData(), secondEncryptionKey.GetSize() }, data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(EncryptionKeySuite, EraseTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::EncryptionKey encryptionKey{ Security::Buffer{ data } };

    EXPECT_EQ(encryptionKey.GetSize(), data.size());
    encryptionKey.Erase();
    EXPECT_TRUE(encryptionKey.IsEmpty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SignatureKeySuite, ViewConstructorTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SignatureKey const signatureKey{ data };
    EXPECT_FALSE(signatureKey.IsEmpty());
    EXPECT_EQ(signatureKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(std::span{ signatureKey.GetData(), signatureKey.GetSize() }, data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SignatureKeySuite, MoveConstructorTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SignatureKey firstSignatureKey{ data };
    Security::SignatureKey const secondSignatureKey{ std::move(firstSignatureKey) };
    EXPECT_TRUE(firstSignatureKey.IsEmpty());
    EXPECT_EQ(secondSignatureKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(std::span{ secondSignatureKey.GetData(), secondSignatureKey.GetSize() }, data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SignatureKeySuite, MoveAssignmentOperatorTest) 
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SignatureKey firstSignatureKey{ data };
    Security::SignatureKey const secondSignatureKey = std::move(firstSignatureKey);
    EXPECT_TRUE(firstSignatureKey.IsEmpty());
    EXPECT_EQ(secondSignatureKey.GetSize(), data.size());
    EXPECT_TRUE(std::ranges::equal(std::span{ secondSignatureKey.GetData(), secondSignatureKey.GetSize() }, data));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(SignatureKeySuite, EraseTest)
{
    auto const data = Security::Test::GenerateGarbageData(65536);
    Security::SignatureKey signatureKey{ Security::Buffer{ data } };

    EXPECT_EQ(signatureKey.GetSize(), data.size());
    signatureKey.Erase();
    EXPECT_TRUE(signatureKey.IsEmpty());
}

//----------------------------------------------------------------------------------------------------------------------
