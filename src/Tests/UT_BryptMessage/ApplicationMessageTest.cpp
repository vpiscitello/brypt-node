//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/PackUtils.hpp"
#include "Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
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

CMessageContext GenerateMessageContext();

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

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BaseConstructorTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    auto const optMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSourceIdentifier(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_FALSE(optMessage->GetAwaitTrackerKey());
    EXPECT_EQ(optMessage->GetCommand(), test::Command);
    EXPECT_EQ(optMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optMessage->GetTimestamp(), TimeUtils::Timestamp());

    auto const buffer = optMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, PackConstructorTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Data)
        .ValidatedBuild();

    auto const pack = optBaseMessage->GetPack();
    EXPECT_EQ(pack.size(), optBaseMessage->GetPackSize());

    auto const optPackMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
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
    EXPECT_EQ(optPackMessage->GetPayload(), optBaseMessage->GetPayload());

    auto const buffer = optPackMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//-----------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitConstructorTest)
{
    CMessageContext const context = local::GenerateMessageContext();
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optSourceBoundMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optSourceBoundMessage);

    EXPECT_EQ(optSourceBoundMessage->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetAwaitTrackerKey(), awaitTrackingKey);
    EXPECT_EQ(optSourceBoundMessage->GetCommand(), test::Command);
    EXPECT_EQ(optSourceBoundMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optSourceBoundMessage->GetTimestamp(), TimeUtils::Timestamp());

    auto const sourceBoundBuffer = optSourceBoundMessage->GetPayload();
    std::string const sourceBoundData(sourceBoundBuffer.begin(), sourceBoundBuffer.end());
    EXPECT_EQ(sourceBoundData, test::Data);

    auto const sourceBoundPack = optSourceBoundMessage->GetPack();
    EXPECT_EQ(sourceBoundPack.size(), optSourceBoundMessage->GetPackSize());

    auto const optDestinationBoundMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optDestinationBoundMessage);

    EXPECT_EQ(optDestinationBoundMessage->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetAwaitTrackerKey(), awaitTrackingKey);
    EXPECT_EQ(optDestinationBoundMessage->GetCommand(), test::Command);
    EXPECT_EQ(optDestinationBoundMessage->GetPhase(), test::RequestPhase);
    EXPECT_GT(optDestinationBoundMessage->GetTimestamp(), TimeUtils::Timestamp());

    auto const destinationBoundBuffer = optDestinationBoundMessage->GetPayload();
    std::string const destinationBoundData(destinationBoundBuffer.begin(), destinationBoundBuffer.end());
    EXPECT_EQ(destinationBoundData, test::Data);

    auto const destinationBoundPack = optDestinationBoundMessage->GetPack();
    EXPECT_EQ(destinationBoundPack.size(), optDestinationBoundMessage->GetPackSize());
}

//------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitPackConstructorTest)
{
    CMessageContext const context = local::GenerateMessageContext();
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optBoundMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_EQ(pack.size(), optBoundMessage->GetPackSize());

    auto const optPackMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBoundMessage->GetSourceIdentifier());
    EXPECT_EQ(optPackMessage->GetDestinationIdentifier(), optBoundMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetAwaitTrackerKey(), optBoundMessage->GetAwaitTrackerKey());
    EXPECT_EQ(optPackMessage->GetCommand(), optBoundMessage->GetCommand());
    EXPECT_EQ(optPackMessage->GetPhase(), optBoundMessage->GetPhase());
    EXPECT_EQ(optPackMessage->GetTimestamp(), optBoundMessage->GetTimestamp());
    EXPECT_EQ(optPackMessage->GetPayload(), optBoundMessage->GetPayload());

    auto const buffer = optPackMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//-----------------------------------------------------------------------------------------------

CMessageContext local::GenerateMessageContext()
{
    CMessageContext context(test::EndpointIdentifier, test::EndpointTechnology);

    context.BindEncryptionHandlers(
        [] (auto const& buffer, auto) -> Security::Encryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); },
        [] (auto const& buffer, auto) -> Security::Decryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); });

    context.BindSignatureHandlers(
        [] (auto&) -> Security::Signator::result_type 
            { return 0; },
        [] (auto const&) -> Security::Verifier::result_type 
            { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type 
            { return 0; });

    return context;
}

//------------------------------------------------------------------------------------------------
