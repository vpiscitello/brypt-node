//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/NetworkMessage.hpp"
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

TEST(NetworkMessageSuite, HandshakeConstructorTest)
{
    auto const optMessage = Message::Network::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSource(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestination());
    EXPECT_EQ(*optMessage->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optMessage->GetType(), Message::Network::Type::Handshake);

    auto const buffer = optMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HeartbeatRequestConstructorTest)
{
    auto const optRequest = Message::Network::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    EXPECT_EQ(optRequest->GetSource(), test::ClientIdentifier);
    ASSERT_TRUE(optRequest->GetDestination());
    EXPECT_EQ(*optRequest->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optRequest->GetType(), Message::Network::Type::HeartbeatRequest);

    auto const pack = optRequest->GetPack();
    EXPECT_EQ(pack.size(), optRequest->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HeartbeatResponseConstructorTest)
{
    auto const optResponse = Message::Network::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_EQ(optResponse->GetSource(), test::ClientIdentifier);
    ASSERT_TRUE(optResponse->GetDestination());
    EXPECT_EQ(*optResponse->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetType(), Message::Network::Type::HeartbeatResponse);

    auto const pack = optResponse->GetPack();
    EXPECT_EQ(pack.size(), optResponse->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HandshakePackConstructorTest)
{
    auto const optBaseMessage = Message::Network::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Network::Parcel::GetBuilder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSource(), optBaseMessage->GetSource());
    ASSERT_TRUE(optPackMessage->GetDestination());
    EXPECT_EQ(*optPackMessage->GetDestination(), *optBaseMessage->GetDestination());
    EXPECT_EQ(optPackMessage->GetType(), optBaseMessage->GetType());
    EXPECT_EQ(optPackMessage->GetPayload(), optBaseMessage->GetPayload());

    auto const buffer = optPackMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HeartbeatRequestPackConstructorTest)
{
    auto const optBaseMessage = Message::Network::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Network::Parcel::GetBuilder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSource(), optBaseMessage->GetSource());
    ASSERT_TRUE(optPackMessage->GetDestination());
    EXPECT_EQ(*optPackMessage->GetDestination(), *optBaseMessage->GetDestination());
    EXPECT_EQ(optPackMessage->GetType(), optBaseMessage->GetType());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HeartbeatResponsePackConstructorTest)
{
    auto const optBaseMessage = Message::Network::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Network::Parcel::GetBuilder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSource(), optBaseMessage->GetSource());
    ASSERT_TRUE(optPackMessage->GetDestination());
    EXPECT_EQ(*optPackMessage->GetDestination(), *optBaseMessage->GetDestination());
    EXPECT_EQ(optPackMessage->GetType(), optBaseMessage->GetType());
}

//----------------------------------------------------------------------------------------------------------------------
