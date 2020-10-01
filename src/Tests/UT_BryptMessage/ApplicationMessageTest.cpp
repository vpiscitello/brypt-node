//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/PackUtils.hpp"
#include "../../BryptMessage/MessageSecurity.hpp"
#include "../../Utilities/TimeUtils.hpp"
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

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ServerIdentifier(BryptIdentifier::Generate());

constexpr Command::Type Command = Command::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Data = "Hello World!";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CApplicationMessageSuite, BaseConstructorTest)
{
    auto const optMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSourceIdentifier(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_FALSE(optMessage->GetAwaitTrackerKey());
    EXPECT_EQ(optMessage->GetCommand(), test::Command);
    EXPECT_EQ(optMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optMessage->GetTimestamp(), TimeUtils::Timestamp());

    auto const buffer = optMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//------------------------------------------------------------------------------------------------

TEST(CApplicationMessageSuite, PackConstructorTest)
{
    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Data)
        .ValidatedBuild();

    auto const pack = optBaseMessage->GetPack();
    EXPECT_EQ(pack.size(), optBaseMessage->GetPackSize());

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBaseMessage->GetSourceIdentifier());
    ASSERT_TRUE(optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetDestinationIdentifier(), optBaseMessage->GetDestinationIdentifier());
    EXPECT_FALSE(optPackMessage->GetAwaitTrackerKey());
    EXPECT_EQ(optPackMessage->GetCommand(), optBaseMessage->GetCommand());
    EXPECT_EQ(optPackMessage->GetPhase(), optBaseMessage->GetPhase());
    EXPECT_EQ(optPackMessage->GetTimestamp(), optBaseMessage->GetTimestamp());
    EXPECT_EQ(optPackMessage->GetData(), optBaseMessage->GetData());

    auto const buffer = optPackMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//-----------------------------------------------------------------------------------------------

TEST(CApplicationMessageSuite, BoundAwaitConstructorTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optSourceBoundMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optSourceBoundMessage);

    EXPECT_EQ(optSourceBoundMessage->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetAwaitTrackerKey(), awaitTrackingKey);
    EXPECT_EQ(optSourceBoundMessage->GetCommand(), test::Command);
    EXPECT_EQ(optSourceBoundMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optSourceBoundMessage->GetTimestamp(), TimeUtils::Timestamp());

    auto const sourceBoundBuffer = optSourceBoundMessage->GetData();
    std::string const sourceBoundData(sourceBoundBuffer.begin(), sourceBoundBuffer.end());
    EXPECT_EQ(sourceBoundData, test::Data);

    auto const sourceBoundPack = optSourceBoundMessage->GetPack();
    EXPECT_EQ(sourceBoundPack.size(), optSourceBoundMessage->GetPackSize());

    auto const optDestinationBoundMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optDestinationBoundMessage);

    EXPECT_EQ(optDestinationBoundMessage->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetAwaitTrackerKey(), awaitTrackingKey);
    EXPECT_EQ(optDestinationBoundMessage->GetCommand(), test::Command);
    EXPECT_EQ(optDestinationBoundMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optDestinationBoundMessage->GetTimestamp(), TimeUtils::Timestamp());

    auto const destinationBoundBuffer = optDestinationBoundMessage->GetData();
    std::string const destinationBoundData(destinationBoundBuffer.begin(), destinationBoundBuffer.end());
    EXPECT_EQ(destinationBoundData, test::Data);

    auto const destinationBoundPack = optDestinationBoundMessage->GetPack();
    EXPECT_EQ(destinationBoundPack.size(), optDestinationBoundMessage->GetPackSize());
}

//------------------------------------------------------------------------------------------------

TEST(CApplicationMessageSuite, BoundAwaitPackConstructorTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optBoundMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_EQ(pack.size(), optBoundMessage->GetPackSize());

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBoundMessage->GetSourceIdentifier());
    EXPECT_EQ(optPackMessage->GetDestinationIdentifier(), optBoundMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetAwaitTrackerKey(), optBoundMessage->GetAwaitTrackerKey());
    EXPECT_EQ(optPackMessage->GetCommand(), optBoundMessage->GetCommand());
    EXPECT_EQ(optPackMessage->GetPhase(), optBoundMessage->GetPhase());
    EXPECT_EQ(optPackMessage->GetTimestamp(), optBoundMessage->GetTimestamp());
    EXPECT_EQ(optPackMessage->GetData(), optBoundMessage->GetData());

    auto const buffer = optPackMessage->GetData();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//-----------------------------------------------------------------------------------------------

TEST(CApplicationMessageSuite, BaseVerificationTest)
{
    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const baseBuffer = PackUtils::Z85Decode(optBaseMessage->GetPack());
    auto const baseStatus = MessageSecurity::Verify(baseBuffer);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromEncodedPack(optBaseMessage->GetPack())
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    auto const packBuffer = PackUtils::Z85Decode(optPackMessage->GetPack());
    auto const packStatus = MessageSecurity::Verify(packBuffer);
    EXPECT_EQ(packStatus, MessageSecurity::VerificationStatus::Success);
}

//-----------------------------------------------------------------------------------------------

TEST(CApplicationMessageSuite, ExtensionVerificationTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto const baseBuffer = PackUtils::Z85Decode(optBaseMessage->GetPack());
    auto const baseStatus = MessageSecurity::Verify(baseBuffer);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromEncodedPack(optBaseMessage->GetPack())
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    auto const packBuffer = PackUtils::Z85Decode(optPackMessage->GetPack());
    auto const packStatus = MessageSecurity::Verify(packBuffer);
    EXPECT_EQ(packStatus, MessageSecurity::VerificationStatus::Success);
}

//-----------------------------------------------------------------------------------------------

TEST(CApplicationMessageSuite, AlteredVerificationTest)
{
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    auto pack = optBaseMessage->GetPack();
    auto const baseBuffer = PackUtils::Z85Decode(pack);
    auto const baseStatus = MessageSecurity::Verify(baseBuffer);
    EXPECT_EQ(baseStatus, MessageSecurity::VerificationStatus::Success);

    std::replace(pack.begin(), pack.end(), pack.at(pack.size() / 2), '?');

    auto const optPackMessage = CApplicationMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_FALSE(optPackMessage);
}

//-----------------------------------------------------------------------------------------------