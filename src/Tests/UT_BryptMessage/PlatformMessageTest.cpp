//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/PlatformMessage.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <string_view>
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

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
Node::Identifier const ServerIdentifier(Node::GenerateIdentifier());

constexpr std::string_view Data = "Hello World!";

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(PlatformMessageSuite, HandshakeConstructorTest)
{
    auto const optMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSource(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestination());
    EXPECT_EQ(*optMessage->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optMessage->GetType(), Message::Platform::ParcelType::Handshake);
    EXPECT_EQ(optMessage->GetPayload().GetStringView(), test::Data);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PlatformMessageSuite, HeartbeatRequestConstructorTest)
{
    auto const optRequest = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    EXPECT_EQ(optRequest->GetSource(), test::ClientIdentifier);
    ASSERT_TRUE(optRequest->GetDestination());
    EXPECT_EQ(*optRequest->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optRequest->GetType(), Message::Platform::ParcelType::HeartbeatRequest);

    auto const pack = optRequest->GetPack();
    EXPECT_EQ(pack.size(), optRequest->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PlatformMessageSuite, HeartbeatResponseConstructorTest)
{
    auto const optResponse = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_EQ(optResponse->GetSource(), test::ClientIdentifier);
    ASSERT_TRUE(optResponse->GetDestination());
    EXPECT_EQ(*optResponse->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetType(), Message::Platform::ParcelType::HeartbeatResponse);

    auto const pack = optResponse->GetPack();
    EXPECT_EQ(pack.size(), optResponse->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PlatformMessageSuite, HandshakePackConstructorTest)
{
    auto const optBaseMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Platform::Parcel::GetBuilder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);
    EXPECT_EQ(optPackMessage, optBaseMessage);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PlatformMessageSuite, HeartbeatRequestPackConstructorTest)
{
    auto const optBaseMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Platform::Parcel::GetBuilder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);
    EXPECT_EQ(optPackMessage, optBaseMessage);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PlatformMessageSuite, HeartbeatResponsePackConstructorTest)
{
    auto const optBaseMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Platform::Parcel::GetBuilder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);
    EXPECT_EQ(optPackMessage, optBaseMessage);
}

//----------------------------------------------------------------------------------------------------------------------
