//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
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

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CMessageSuite, BaseMessageParameterConstructorTest)
{
    auto const optMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSourceIdentifier(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_FALSE(optMessage->GetAwaitingKey());
    EXPECT_EQ(optMessage->GetCommand(), test::Command);
    EXPECT_EQ(optMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optMessage->GetTimepoint(), TimeUtils::Timepoint());

    auto const buffer = optMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Message);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//------------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundAwaitMessageParameterConstructorTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optSourceBoundMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optSourceBoundMessage);

    EXPECT_EQ(optSourceBoundMessage->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetAwaitingKey(), awaitTrackingKey);
    EXPECT_EQ(optSourceBoundMessage->GetCommand(), test::Command);
    EXPECT_EQ(optSourceBoundMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optSourceBoundMessage->GetTimepoint(), TimeUtils::Timepoint());

    auto const sourceBoundBuffer = optSourceBoundMessage->GetData();
    std::string const sourceBoundData(sourceBoundBuffer.begin(), sourceBoundBuffer.end());
    EXPECT_EQ(sourceBoundData, test::Message);

    auto const sourceBoundPack = optSourceBoundMessage->GetPack();
    EXPECT_EQ(sourceBoundPack.size(), optSourceBoundMessage->GetPackSize());

    auto const optDestinationBoundMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optDestinationBoundMessage);

    EXPECT_EQ(optDestinationBoundMessage->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetAwaitingKey(), awaitTrackingKey);
    EXPECT_EQ(optDestinationBoundMessage->GetCommand(), test::Command);
    EXPECT_EQ(optDestinationBoundMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optDestinationBoundMessage->GetTimepoint(), TimeUtils::Timepoint());

    auto const destinationBoundBuffer = optDestinationBoundMessage->GetData();
    std::string const destinationBoundData(destinationBoundBuffer.begin(), destinationBoundBuffer.end());
    EXPECT_EQ(destinationBoundData, test::Message);

    auto const destinationBoundPack = optDestinationBoundMessage->GetPack();
    EXPECT_EQ(destinationBoundPack.size(), optDestinationBoundMessage->GetPackSize());
}

//------------------------------------------------------------------------------------------------

TEST(CMessageSuite, BaseMessagePackConstructorTest)
{
    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .ValidatedBuild();

    auto const pack = optBaseMessage->GetPack();
    EXPECT_EQ(pack.size(), optBaseMessage->GetPackSize());

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optBaseMessage->GetSourceIdentifier(), optPackMessage->GetSourceIdentifier());
    EXPECT_EQ(optBaseMessage->GetDestinationIdentifier(), optPackMessage->GetDestinationIdentifier());
    EXPECT_FALSE(optBaseMessage->GetAwaitingKey());
    EXPECT_EQ(optBaseMessage->GetCommand(), optPackMessage->GetCommand());
    EXPECT_EQ(optBaseMessage->GetPhase(), optPackMessage->GetPhase());
    EXPECT_GT(optBaseMessage->GetTimepoint(), optPackMessage->GetTimepoint());

    auto const buffer = optPackMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Message);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundMessagePackConstructorTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optBoundMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_EQ(pack.size(), optBoundMessage->GetPackSize());

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optBoundMessage->GetSourceIdentifier(), optPackMessage->GetSourceIdentifier());
    EXPECT_EQ(optBoundMessage->GetDestinationIdentifier(), optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(optBoundMessage->GetAwaitingKey(), optPackMessage->GetAwaitingKey());
    EXPECT_EQ(optBoundMessage->GetCommand(), optPackMessage->GetCommand());
    EXPECT_EQ(optBoundMessage->GetPhase(), optPackMessage->GetPhase());
    EXPECT_GT(optBoundMessage->GetTimepoint(), optPackMessage->GetTimepoint());

    auto const buffer = optPackMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Message);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BoundMessageBufferConstructorTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optBoundMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_EQ(pack.size(), optBoundMessage->GetPackSize());

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optBoundMessage->GetSourceIdentifier(), optPackMessage->GetSourceIdentifier());
    EXPECT_EQ(optBoundMessage->GetDestinationIdentifier(), optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(optBoundMessage->GetAwaitingKey(), optPackMessage->GetAwaitingKey());
    EXPECT_EQ(optBoundMessage->GetCommand(), optPackMessage->GetCommand());
    EXPECT_EQ(optBoundMessage->GetPhase(), optPackMessage->GetPhase());
    EXPECT_GT(optBoundMessage->GetTimepoint(), optPackMessage->GetTimepoint());

    auto const buffer = optPackMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Message);
}

//-----------------------------------------------------------------------------------------------

TEST(CMessageSuite, BaseMessageVerificationTest)
{
    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const baseStatus = MessageSecurity::Verify(*optBaseMessage);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    auto const optPackMessage = CApplicationMessage::Builder()
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

    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const baseStatus = MessageSecurity::Verify(*optBaseMessage);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    auto const optPackMessage = CApplicationMessage::Builder()
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

    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto pack = optBaseMessage->GetPack();
    auto const baseStatus = MessageSecurity::Verify(pack);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    std::replace(pack.begin(), pack.end(), pack.at(pack.size() / 2), '?');

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromPack(pack)
        .ValidatedBuild();
    ASSERT_FALSE(optPackMessage);
}

//-----------------------------------------------------------------------------------------------