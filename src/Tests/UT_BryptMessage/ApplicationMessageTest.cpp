//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/PackUtils.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

MessageContext GenerateMessageContext();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
Node::Identifier const ServerIdentifier(Node::GenerateIdentifier());

constexpr std::string_view RequestRoute = "/request";
constexpr std::string_view Data = "Hello World!";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BaseConstructorTest)
{
    MessageContext const context = local::GenerateMessageContext();

    auto const optMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSourceIdentifier(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestinationIdentifier());
    EXPECT_EQ(*optMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optMessage->GetRoute(), test::RequestRoute);
    EXPECT_FALSE(optMessage->GetAwaitTrackerKey());

    auto const buffer = optMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, PackConstructorTest)
{
    MessageContext const context = local::GenerateMessageContext();

    auto const optBaseMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .ValidatedBuild();

    auto const pack = optBaseMessage->GetPack();
    EXPECT_EQ(pack.size(), optBaseMessage->GetPackSize());

    auto const optPackMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBaseMessage->GetSourceIdentifier());
    ASSERT_TRUE(optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetDestinationIdentifier(), optBaseMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetRoute(), optBaseMessage->GetRoute());
    EXPECT_EQ(optPackMessage->GetPayload(), optBaseMessage->GetPayload());
    EXPECT_FALSE(optPackMessage->GetAwaitTrackerKey());

    auto const buffer = optPackMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitConstructorTest)
{
    MessageContext const context = local::GenerateMessageContext();
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optSourceBoundMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optSourceBoundMessage);

    EXPECT_EQ(optSourceBoundMessage->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optSourceBoundMessage->GetRoute(), test::RequestRoute);
    EXPECT_EQ(optSourceBoundMessage->GetAwaitTrackerKey(), awaitTrackingKey);

    auto const sourceBoundBuffer = optSourceBoundMessage->GetPayload();
    std::string const sourceBoundData(sourceBoundBuffer.begin(), sourceBoundBuffer.end());
    EXPECT_EQ(sourceBoundData, test::Data);

    auto const sourceBoundPack = optSourceBoundMessage->GetPack();
    EXPECT_EQ(sourceBoundPack.size(), optSourceBoundMessage->GetPackSize());

    auto const optDestinationBoundMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optDestinationBoundMessage);

    EXPECT_EQ(optDestinationBoundMessage->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optDestinationBoundMessage->GetAwaitTrackerKey(), awaitTrackingKey);
    EXPECT_EQ(optDestinationBoundMessage->GetRoute(), test::RequestRoute);

    auto const destinationBoundBuffer = optDestinationBoundMessage->GetPayload();
    std::string const destinationBoundData(destinationBoundBuffer.begin(), destinationBoundBuffer.end());
    EXPECT_EQ(destinationBoundData, test::Data);

    auto const destinationBoundPack = optDestinationBoundMessage->GetPack();
    EXPECT_EQ(destinationBoundPack.size(), optDestinationBoundMessage->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitPackConstructorTest)
{
    MessageContext const context = local::GenerateMessageContext();
    Await::TrackerKey const awaitTrackingKey = 0x89ABCDEF;

    auto const optBoundMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_EQ(pack.size(), optBoundMessage->GetPackSize());

    auto const optPackMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBoundMessage->GetSourceIdentifier());
    EXPECT_EQ(optPackMessage->GetDestinationIdentifier(), optBoundMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetAwaitTrackerKey(), optBoundMessage->GetAwaitTrackerKey());
    EXPECT_EQ(optPackMessage->GetRoute(), optBoundMessage->GetRoute());
    EXPECT_EQ(optPackMessage->GetPayload(), optBoundMessage->GetPayload());

    auto const buffer = optPackMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//----------------------------------------------------------------------------------------------------------------------

MessageContext local::GenerateMessageContext()
{
    MessageContext context(test::EndpointIdentifier, test::EndpointProtocol);

    context.BindEncryptionHandlers(
        [] (auto const& buffer, auto) -> Security::Encryptor::result_type { 
            return Security::Buffer(buffer.begin(), buffer.end()); 
        },
        [] (auto const& buffer, auto) -> Security::Decryptor::result_type {
            return Security::Buffer(buffer.begin(), buffer.end());
        });

    context.BindSignatureHandlers(
        [] (auto&) -> Security::Signator::result_type { return 0; },
        [] (auto const&) -> Security::Verifier::result_type { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type { return 0; });

    return context;
}

//----------------------------------------------------------------------------------------------------------------------
