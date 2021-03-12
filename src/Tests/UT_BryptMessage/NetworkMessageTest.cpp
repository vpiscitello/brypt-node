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

BryptIdentifier::Container const ClientIdentifier(BryptIdentifier::Generate());
BryptIdentifier::Container const ServerIdentifier(BryptIdentifier::Generate());

constexpr std::string_view Data = "Hello World!";

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HandshakeConstructorTest)
{
    auto const optMessage = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSourceIdentifier(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optMessage->GetMessageType(), Message::Network::Type::Handshake);

    auto const buffer = optMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HeartbeatRequestConstructorTest)
{
    auto const optRequest = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    EXPECT_EQ(optRequest->GetSourceIdentifier(), test::ClientIdentifier);
    ASSERT_TRUE(optRequest->GetDestinationIdentifier());
    EXPECT_EQ(*optRequest->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optRequest->GetMessageType(), Message::Network::Type::HeartbeatRequest);

    auto const pack = optRequest->GetPack();
    EXPECT_EQ(pack.size(), optRequest->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HeartbeatResponseConstructorTest)
{
    auto const optResponse = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_EQ(optResponse->GetSourceIdentifier(), test::ClientIdentifier);
    ASSERT_TRUE(optResponse->GetDestinationIdentifier());
    EXPECT_EQ(*optResponse->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetMessageType(), Message::Network::Type::HeartbeatResponse);

    auto const pack = optResponse->GetPack();
    EXPECT_EQ(pack.size(), optResponse->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HandshakePackConstructorTest)
{
    auto const optBaseMessage = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = NetworkMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBaseMessage->GetSourceIdentifier());
    ASSERT_TRUE(optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optPackMessage->GetDestinationIdentifier(), *optBaseMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetMessageType(), optBaseMessage->GetMessageType());
    EXPECT_EQ(optPackMessage->GetPayload(), optBaseMessage->GetPayload());

    auto const buffer = optPackMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HeartbeatRequestPackConstructorTest)
{
    auto const optBaseMessage = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = NetworkMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBaseMessage->GetSourceIdentifier());
    ASSERT_TRUE(optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optPackMessage->GetDestinationIdentifier(), *optBaseMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetMessageType(), optBaseMessage->GetMessageType());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkMessageSuite, HeartbeatResponsePackConstructorTest)
{
    auto const optBaseMessage = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = NetworkMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBaseMessage->GetSourceIdentifier());
    ASSERT_TRUE(optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optPackMessage->GetDestinationIdentifier(), *optBaseMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetMessageType(), optBaseMessage->GetMessageType());
}

//----------------------------------------------------------------------------------------------------------------------
