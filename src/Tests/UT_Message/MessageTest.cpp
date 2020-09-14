//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "../../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
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

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ServerIdentifier(BryptIdentifier::Generate());

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
    OptionalMessage const optMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSource(), test::ClientIdentifier);
    EXPECT_EQ(optMessage->GetDestination(), test::ServerIdentifier);
    EXPECT_FALSE(optMessage->GetAwaitingKey());
    EXPECT_EQ(optMessage->GetCommandType(), test::Command);
    EXPECT_EQ(optMessage->GetPhase(), test::RequestPhase);
    EXPECT_EQ(optMessage->GetNonce(), test::Nonce);
    EXPECT_GT(optMessage->GetSystemTimepoint(), TimeUtils::Timepoint());

    auto const optDecrypted = optMessage->GetDecryptedData();
    ASSERT_TRUE(optDecrypted);
    std::string const data(optDecrypted->begin(), optDecrypted->end());
    EXPECT_EQ(data, test::Message);

    auto const pack = optMessage->GetPack();
    EXPECT_GT(pack.size(), CMessage::FixedPackSize());
}

//------------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundAwaitMessageParameterConstructorTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    OptionalMessage const optSourceBoundMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optSourceBoundMessage);

    EXPECT_EQ(optSourceBoundMessage->GetSource(), test::ClientIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetAwaitingKey(), awaitTrackingKey);
    EXPECT_EQ(optSourceBoundMessage->GetCommandType(), test::Command);
    EXPECT_EQ(optSourceBoundMessage->GetPhase(), test::RequestPhase);
    EXPECT_EQ(optSourceBoundMessage->GetNonce(), test::Nonce);
    EXPECT_GT(optSourceBoundMessage->GetSystemTimepoint(), TimeUtils::Timepoint());

    auto const optSourceBoundDecrypted = optSourceBoundMessage->GetDecryptedData();
    ASSERT_TRUE(optSourceBoundDecrypted);

    std::string const sourceBoundData(optSourceBoundDecrypted->begin(), optSourceBoundDecrypted->end());
    EXPECT_EQ(sourceBoundData, test::Message);

    std::string const sourceBoundPack = optSourceBoundMessage->GetPack();
    EXPECT_GT(sourceBoundPack.size(), CMessage::FixedPackSize());

    OptionalMessage const optDestinationBoundMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optDestinationBoundMessage);

    EXPECT_EQ(optDestinationBoundMessage->GetSource(), test::ClientIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetAwaitingKey(), awaitTrackingKey);
    EXPECT_EQ(optDestinationBoundMessage->GetCommandType(), test::Command);
    EXPECT_EQ(optDestinationBoundMessage->GetPhase(), test::RequestPhase);
    EXPECT_EQ(optDestinationBoundMessage->GetNonce(), test::Nonce);
    EXPECT_GT(optDestinationBoundMessage->GetSystemTimepoint(), TimeUtils::Timepoint());

    auto const optDestinationBoundDecrypted = optDestinationBoundMessage->GetDecryptedData();
    ASSERT_TRUE(optDestinationBoundDecrypted);

    std::string const destinationBoundData(optDestinationBoundDecrypted->begin(), optDestinationBoundDecrypted->end());
    EXPECT_EQ(destinationBoundData, test::Message);

    std::string const destinationBoundPack = optDestinationBoundMessage->GetPack();
    EXPECT_GT(destinationBoundPack.size(), CMessage::FixedPackSize());
}

// //------------------------------------------------------------------------------------------------

TEST(CMessageSuite, BaseMessagePackConstructorTest)
{
    OptionalMessage const optBaseMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    auto const pack = optBaseMessage->GetPack();
    EXPECT_GT(pack.size(), CMessage::FixedPackSize());

    OptionalMessage const optPackMessage = CMessage::Builder()
        .FromPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optBaseMessage->GetSource(), optPackMessage->GetSource());
    EXPECT_EQ(optBaseMessage->GetDestination(), optPackMessage->GetDestination());
    EXPECT_FALSE(optBaseMessage->GetAwaitingKey());
    EXPECT_EQ(optBaseMessage->GetCommandType(), optPackMessage->GetCommandType());
    EXPECT_EQ(optBaseMessage->GetPhase(), optPackMessage->GetPhase());
    EXPECT_EQ(optBaseMessage->GetNonce(), optPackMessage->GetNonce());
    EXPECT_GT(optBaseMessage->GetSystemTimepoint(), optPackMessage->GetSystemTimepoint());

    auto const optDecrypted = optPackMessage->GetDecryptedData();
    ASSERT_TRUE(optDecrypted);
    std::string const data(optDecrypted->begin(), optDecrypted->end());
    EXPECT_EQ(data, test::Message);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundMessagePackConstructorTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    OptionalMessage const optBoundMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_GT(pack.size(), CMessage::FixedPackSize());

    OptionalMessage const optPackMessage = CMessage::Builder()
        .FromPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optBoundMessage->GetSource(), optPackMessage->GetSource());
    EXPECT_EQ(optBoundMessage->GetDestination(), optPackMessage->GetDestination());
    EXPECT_EQ(optBoundMessage->GetAwaitingKey(), optPackMessage->GetAwaitingKey());
    EXPECT_EQ(optBoundMessage->GetCommandType(), optPackMessage->GetCommandType());
    EXPECT_EQ(optBoundMessage->GetPhase(), optPackMessage->GetPhase());
    EXPECT_EQ(optBoundMessage->GetNonce(), optPackMessage->GetNonce());
    EXPECT_GT(optBoundMessage->GetSystemTimepoint(), optPackMessage->GetSystemTimepoint());

    auto const optDecrypted = optPackMessage->GetDecryptedData();
    ASSERT_TRUE(optDecrypted);
    std::string const data(optDecrypted->begin(), optDecrypted->end());
    EXPECT_EQ(data, test::Message);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundMessageBufferConstructorTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    OptionalMessage const optBoundMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    Message::Buffer buffer(pack.begin(), pack.end());

    EXPECT_GT(buffer.size(), CMessage::FixedPackSize());
    EXPECT_EQ(buffer.size(), pack.size());

    OptionalMessage const optPackMessage = CMessage::Builder()
        .FromPack(buffer)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optBoundMessage->GetSource(), optPackMessage->GetSource());
    EXPECT_EQ(optBoundMessage->GetDestination(), optPackMessage->GetDestination());
    EXPECT_EQ(optBoundMessage->GetAwaitingKey(), optPackMessage->GetAwaitingKey());
    EXPECT_EQ(optBoundMessage->GetCommandType(), optPackMessage->GetCommandType());
    EXPECT_EQ(optBoundMessage->GetPhase(), optPackMessage->GetPhase());
    EXPECT_EQ(optBoundMessage->GetNonce(), optPackMessage->GetNonce());
    EXPECT_GT(optBoundMessage->GetSystemTimepoint(), optPackMessage->GetSystemTimepoint());

    auto const optDecrypted = optPackMessage->GetDecryptedData();
    ASSERT_TRUE(optDecrypted);
    std::string const data(optDecrypted->begin(), optDecrypted->end());
    EXPECT_EQ(data, test::Message);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BaseMessageVerificationTest)
{
    OptionalMessage const optBaseMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const baseStatus = MessageSecurity::Verify(*optBaseMessage);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    OptionalMessage const optPackMessage = CMessage::Builder()
        .FromPack(optBaseMessage->GetPack())
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    auto const packStatus = MessageSecurity::Verify(*optPackMessage);
    EXPECT_EQ(packStatus, MessageSecurity::VerificationStatus::Success);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundMessageVerificationTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    OptionalMessage const optBaseMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const baseStatus = MessageSecurity::Verify(*optBaseMessage);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    OptionalMessage const optPackMessage = CMessage::Builder()
        .FromPack(optBaseMessage->GetPack())
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    auto const packStatus = MessageSecurity::Verify(*optPackMessage);
    EXPECT_EQ(packStatus, MessageSecurity::VerificationStatus::Success);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, AlteredMessageVerificationTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    OptionalMessage const optBaseMessage = CMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto pack = optBaseMessage->GetPack();
    auto const baseStatus = MessageSecurity::Verify(pack);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    std::replace(pack.begin(), pack.end(), pack.at(pack.size() / 2), '?');

    OptionalMessage const optPackMessage = CMessage::Builder()
        .FromPack(pack)
        .ValidatedBuild();
    ASSERT_FALSE(optPackMessage);
}

//-----------------------------------------------------------------------------------------------