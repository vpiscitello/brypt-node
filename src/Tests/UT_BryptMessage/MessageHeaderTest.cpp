//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/PlatformMessage.hpp"
#include "BryptMessage/MessageUtils.hpp"
#include "Utilities/Z85.hpp"
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

Message::Context GenerateMessageContext();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
Node::Identifier const ServerIdentifier(Node::GenerateIdentifier());

constexpr std::string_view RequestRoute = "/request";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ApplicationConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    Message::Header const header = optMessage->GetHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSource(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(header.GetDestination());
    EXPECT_EQ(*header.GetDestination(), test::ServerIdentifier);
    EXPECT_GT(header.GetTimestamp(), TimeUtils::Timestamp());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ApplicationPackTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optBaseMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    Message::Header const baseHeader = optBaseMessage->GetHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(baseHeader.GetSource(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(baseHeader.GetDestination());
    EXPECT_EQ(*baseHeader.GetDestination(), test::ServerIdentifier);
    EXPECT_GT(baseHeader.GetTimestamp(), TimeUtils::Timestamp());

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    Message::Header const packHeader = optPackMessage->GetHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSource(), baseHeader.GetSource());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    ASSERT_TRUE(packHeader.GetDestination());
    EXPECT_EQ(*packHeader.GetDestination(), *baseHeader.GetDestination());
    EXPECT_EQ(packHeader.GetTimestamp(), baseHeader.GetTimestamp());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkConstructorTest)
{
    auto const optMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    Message::Header const header = optMessage->GetHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Platform);
    EXPECT_EQ(header.GetSource(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(header.GetDestination());
    EXPECT_EQ(*header.GetDestination(), test::ServerIdentifier);
    EXPECT_GT(header.GetTimestamp(), TimeUtils::Timestamp());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkPackTest)
{
    auto const optBaseMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    Message::Header const baseHeader = optBaseMessage->GetHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Platform);
    EXPECT_EQ(baseHeader.GetSource(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(baseHeader.GetDestination());
    EXPECT_EQ(*baseHeader.GetDestination(), test::ServerIdentifier);
    EXPECT_GT(baseHeader.GetTimestamp(), TimeUtils::Timestamp());

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Platform::Parcel::GetBuilder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    Message::Header const packHeader = optPackMessage->GetHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSource(), baseHeader.GetSource());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    ASSERT_TRUE(packHeader.GetDestination());
    EXPECT_EQ(*packHeader.GetDestination(), *baseHeader.GetDestination());
    EXPECT_EQ(packHeader.GetTimestamp(), baseHeader.GetTimestamp());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ClusterDestinationTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetRoute(test::RequestRoute)
        .MakeClusterMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    Message::Header const header = optMessage->GetHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSource(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Cluster);
    EXPECT_FALSE(header.GetDestination());
    EXPECT_GT(header.GetTimestamp(), TimeUtils::Timestamp());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkDestinationTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetRoute(test::RequestRoute)
        .MakeNetworkMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    Message::Header const header = optMessage->GetHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSource(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Network);
    EXPECT_FALSE(header.GetDestination());
    EXPECT_GT(header.GetTimestamp(), TimeUtils::Timestamp());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ClusterPackTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optBaseMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetRoute(test::RequestRoute)
        .MakeClusterMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    Message::Header const baseHeader = optBaseMessage->GetHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(baseHeader.GetSource(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Cluster);
    EXPECT_FALSE(baseHeader.GetDestination());
    EXPECT_GT(baseHeader.GetTimestamp(), TimeUtils::Timestamp());

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    Message::Header const packHeader = optPackMessage->GetHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSource(), baseHeader.GetSource());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    EXPECT_FALSE(packHeader.GetDestination());
    EXPECT_EQ(packHeader.GetTimestamp(), baseHeader.GetTimestamp());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolTest)
{
    Message::Context const context = local::GenerateMessageContext();
    
    auto const optPlatformMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optPlatformMessage);

    auto const networkBuffer = Z85::Decode(optPlatformMessage->GetPack());
    auto const optNetworkProtocol = Message::PeekProtocol(networkBuffer);

    ASSERT_TRUE(optNetworkProtocol);
    EXPECT_EQ(*optNetworkProtocol, Message::Protocol::Platform);

    auto const optApplicationMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationMessage);

    auto const applicationBuffer = Z85::Decode(optApplicationMessage->GetPack());
    auto const optApplicationProtocol = Message::PeekProtocol(applicationBuffer);

    ASSERT_TRUE(optApplicationProtocol);
    EXPECT_EQ(*optApplicationProtocol, Message::Protocol::Application);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolNullBytesTest)
{
    Message::Buffer const buffer(12, 0x00);
    auto const optProtocol = Message::PeekProtocol(buffer);
    EXPECT_FALSE(optProtocol);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolOutOfRangeBytesTest)
{
    Message::Buffer const buffer(12, 0xF0);
    auto const optProtocol = Message::PeekProtocol(buffer);
    EXPECT_FALSE(optProtocol);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolEmptyBufferTest)
{
    Message::Buffer const buffer;
    auto const optProtocol = Message::PeekProtocol(buffer);
    EXPECT_FALSE(optProtocol);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSizeTest)
{
    Message::Context const context = local::GenerateMessageContext();
    
    auto const optPlatformMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optPlatformMessage);

    auto const networkBuffer = Z85::Decode(optPlatformMessage->GetPack());
    auto const optNetworkSize = Message::PeekSize(networkBuffer);

    ASSERT_TRUE(optNetworkSize);
    EXPECT_EQ(*optNetworkSize, optPlatformMessage->GetPack().size());

    auto const optApplicationMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationMessage);

    auto const applicationBuffer = Z85::Decode(optApplicationMessage->GetPack());
    auto const optApplicationSize = Message::PeekSize(applicationBuffer);

    ASSERT_TRUE(optApplicationSize);
    EXPECT_EQ(*optApplicationSize, optApplicationMessage->GetPack().size());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSizeNullBytesTest)
{
    Message::Buffer const buffer(12, 0x00);
    auto const optMessageSize = Message::PeekSize(buffer);
    EXPECT_FALSE(optMessageSize);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSizeEmptyBufferTest)
{
    Message::Buffer const buffer;
    auto const optMessageSize = Message::PeekSize(buffer);
    EXPECT_FALSE(optMessageSize);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceTest)
{
    auto const optMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    auto const buffer = Z85::Decode(optMessage->GetPack());
    auto const optSource = Message::PeekSource(buffer);

    ASSERT_TRUE(optSource);
    EXPECT_EQ(*optSource, test::ClientIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceNullBytesTest)
{
    Message::Buffer const buffer(128, 0x00);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceInvalidIdentifierTest)
{
    Message::Buffer const buffer(128, Node::Identifier::MinimumSize);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceSmallBufferTest)
{
    Message::Buffer const buffer(12, Node::Identifier::MinimumSize);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceSmallIdentifierSizeTest)
{
    Message::Buffer const buffer(128, Node::Identifier::MaximumSize + 1);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceLargeIdentifierSizeTest)
{
    Message::Buffer const buffer(128, Node::Identifier::MinimumSize - 1);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceEmptyBufferTest)
{
    Message::Buffer const buffer;
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context local::GenerateMessageContext()
{
    Message::Context context(test::EndpointIdentifier, test::EndpointProtocol);

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

//----------------------------------------------------------------------------------------------------------------------
