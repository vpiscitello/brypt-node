//------------------------------------------------------------------------------------------------
#include "../../Utilities/Message.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr NodeUtils::NodeIdType ClientId = 0x12345678;
constexpr NodeUtils::NodeIdType ServerId = 0xFFFFFFFF;
constexpr Command::Type Command = Command::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";
constexpr std::uint32_t Nonce = 9999;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CMessageSuite, BaseMessageParameterConstructorTest)
{
    CMessage const message(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);

    EXPECT_EQ(message.GetSourceId(), test::ClientId);
    EXPECT_EQ(message.GetDestinationId(), test::ServerId);
    EXPECT_FALSE(message.GetAwaitId());
    EXPECT_EQ(message.GetCommandType(), test::Command);
    EXPECT_EQ(message.GetPhase(), test::RequestPhase);
    EXPECT_EQ(message.GetNonce(), test::Nonce);
    EXPECT_GT(message.GetSystemTimepoint(), NodeUtils::Timepoint());

    auto const data = message.GetData();
    auto const decrypted = message.Decrypt(data, data.size());
    ASSERT_TRUE(decrypted);
    std::string const str(decrypted->begin(), decrypted->end());
    EXPECT_EQ(str, test::Message);

    auto const pack = message.GetPack();
    EXPECT_GT(pack.size(), std::size_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundAwaitMessageParameterConstructorTest)
{
    NodeUtils::ObjectIdType const awaitKey = 0x89ABCDEF;

    CMessage const sourceBoundMessage(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce,
        Message::BoundAwaitId(Message::AwaitBinding::Source, awaitKey));

    EXPECT_EQ(sourceBoundMessage.GetSourceId(), test::ClientId);
    EXPECT_EQ(sourceBoundMessage.GetDestinationId(), test::ServerId);
    EXPECT_EQ(sourceBoundMessage.GetAwaitId(), awaitKey);
    EXPECT_EQ(sourceBoundMessage.GetCommandType(), test::Command);
    EXPECT_EQ(sourceBoundMessage.GetPhase(), test::RequestPhase);
    EXPECT_EQ(sourceBoundMessage.GetNonce(), test::Nonce);
    EXPECT_GT(sourceBoundMessage.GetSystemTimepoint(), NodeUtils::Timepoint());

    auto const sourceBoundData = sourceBoundMessage.GetData();
    auto const sourceBoundDecrypted = 
        sourceBoundMessage.Decrypt(sourceBoundData, sourceBoundData.size());
    ASSERT_TRUE(sourceBoundDecrypted);

    std::string const sourceBoundStr(sourceBoundDecrypted->begin(), sourceBoundDecrypted->end());
    EXPECT_EQ(sourceBoundStr, test::Message);

    std::string const sourceBoundPack = sourceBoundMessage.GetPack();
    EXPECT_GT(sourceBoundData.size(), std::size_t(0));

    CMessage const destinationBoundMessage(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce,
        Message::BoundAwaitId(Message::AwaitBinding::Destination, awaitKey));

    EXPECT_EQ(destinationBoundMessage.GetSourceId(), test::ClientId);
    EXPECT_EQ(destinationBoundMessage.GetDestinationId(), test::ServerId);
    EXPECT_EQ(destinationBoundMessage.GetAwaitId(), awaitKey);
    EXPECT_EQ(destinationBoundMessage.GetCommandType(), test::Command);
    EXPECT_EQ(destinationBoundMessage.GetPhase(), test::RequestPhase);
    EXPECT_EQ(destinationBoundMessage.GetNonce(), test::Nonce);
    EXPECT_GT(destinationBoundMessage.GetSystemTimepoint(), NodeUtils::Timepoint());

    auto const destinationBoundData = destinationBoundMessage.GetData();
    auto const destinationBoundDecrypted = 
        destinationBoundMessage.Decrypt(destinationBoundData, destinationBoundData.size());
    ASSERT_TRUE(destinationBoundDecrypted);

    std::string const destinationBoundStr(
        destinationBoundDecrypted->begin(), destinationBoundDecrypted->end());
    EXPECT_EQ(destinationBoundStr, test::Message);

    std::string const destinationBoundPack = destinationBoundMessage.GetPack();
    EXPECT_GT(sourceBoundData.size(), std::size_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CMessageSuite, BaseMessagePackConstructorTest)
{
    CMessage const baseMessage(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);

    auto const pack = baseMessage.GetPack();
    EXPECT_GT(pack.size(), std::size_t(0));

    CMessage const packMessage(pack);

    EXPECT_EQ(baseMessage.GetSourceId(), packMessage.GetSourceId());
    EXPECT_EQ(baseMessage.GetDestinationId(), packMessage.GetDestinationId());
    EXPECT_FALSE(baseMessage.GetAwaitId());
    EXPECT_EQ(baseMessage.GetCommandType(), packMessage.GetCommandType());
    EXPECT_EQ(baseMessage.GetPhase(), packMessage.GetPhase());
    EXPECT_EQ(baseMessage.GetNonce(), packMessage.GetNonce());
    EXPECT_GT(baseMessage.GetSystemTimepoint(), packMessage.GetSystemTimepoint());

    auto const data = packMessage.GetData();
    auto const decrypted = packMessage.Decrypt(data, data.size());
    ASSERT_TRUE(decrypted);
    std::string const str(decrypted->begin(), decrypted->end());
    EXPECT_EQ(str, test::Message);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundMessagePackConstructorTest)
{
    NodeUtils::ObjectIdType const awaitKey = 0x89ABCDEF;

    CMessage const boundMessage(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce,
        Message::BoundAwaitId(Message::AwaitBinding::Destination, awaitKey));

    auto const pack = boundMessage.GetPack();
    EXPECT_GT(pack.size(), std::size_t(0));

    CMessage const packMessage(pack);

    EXPECT_EQ(boundMessage.GetSourceId(), packMessage.GetSourceId());
    EXPECT_EQ(boundMessage.GetDestinationId(), packMessage.GetDestinationId());
    EXPECT_EQ(boundMessage.GetAwaitId(), packMessage.GetAwaitId());
    EXPECT_EQ(boundMessage.GetCommandType(), packMessage.GetCommandType());
    EXPECT_EQ(boundMessage.GetPhase(), packMessage.GetPhase());
    EXPECT_EQ(boundMessage.GetNonce(), packMessage.GetNonce());
    EXPECT_GT(boundMessage.GetSystemTimepoint(), packMessage.GetSystemTimepoint());

    auto const data = packMessage.GetData();
    auto const decrypted = packMessage.Decrypt(data, data.size());
    ASSERT_TRUE(decrypted);
    std::string const str(decrypted->begin(), decrypted->end());
    EXPECT_EQ(str, test::Message);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BaseMessageVerificationTest)
{
    CMessage const baseMessage(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);

    auto const pack = baseMessage.GetPack();
    auto const baseStatus = baseMessage.Verify();
    EXPECT_EQ(baseStatus, Message::VerificationStatus::Success);

    CMessage packMessage(pack);
    auto const packStatus = packMessage.Verify();
    EXPECT_EQ(packStatus, Message::VerificationStatus::Success);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundMessageVerificationTest)
{
    NodeUtils::ObjectIdType const awaitKey = 0x89ABCDEF;

    CMessage const baseMessage(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce,
        Message::BoundAwaitId(Message::AwaitBinding::Source, awaitKey));

    auto const pack = baseMessage.GetPack();
    auto const baseStatus = baseMessage.Verify();
    EXPECT_EQ(baseStatus, Message::VerificationStatus::Success);

    CMessage packMessage(pack);
    auto const packStatus = packMessage.Verify();
    EXPECT_EQ(packStatus, Message::VerificationStatus::Success);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, AlteredMessageVerificationTest)
{
    NodeUtils::ObjectIdType const awaitKey = 0x89ABCDEF;

    CMessage const baseMessage(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce,
        Message::BoundAwaitId(Message::AwaitBinding::Source, awaitKey));

    auto pack = baseMessage.GetPack();
    auto const baseStatus = baseMessage.Verify();
    EXPECT_EQ(baseStatus, Message::VerificationStatus::Success);

    std::replace(pack.begin(), pack.end(), pack.at(pack.size() / 2), '?');

    CMessage packMessage(pack);
    auto const packStatus = packMessage.Verify();
    EXPECT_EQ(packStatus, Message::VerificationStatus::Unauthorized);
}

//-----------------------------------------------------------------------------------------------