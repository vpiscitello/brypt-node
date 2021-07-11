//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/NetworkMessage.hpp"
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

MessageContext GenerateMessageContext();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
Node::Identifier const ServerIdentifier(Node::GenerateIdentifier());

constexpr Handler::Type Handler = Handler::Type::Election;
constexpr std::uint8_t Phase = 0;

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ApplicationConstructorTest)
{
    MessageContext const context = local::GenerateMessageContext();

    auto const optMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Handler, test::Phase)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    MessageHeader const header = optMessage->GetMessageHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(header.GetDestinationIdentifier());
    EXPECT_EQ(*header.GetDestinationIdentifier(), test::ServerIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ApplicationPackTest)
{
    MessageContext const context = local::GenerateMessageContext();

    auto const optBaseMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Handler, test::Phase)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    MessageHeader const baseHeader = optBaseMessage->GetMessageHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(baseHeader.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(baseHeader.GetDestinationIdentifier());
    EXPECT_EQ(*baseHeader.GetDestinationIdentifier(), test::ServerIdentifier);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    MessageHeader const packHeader = optPackMessage->GetMessageHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSourceIdentifier(), baseHeader.GetSourceIdentifier());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    ASSERT_TRUE(packHeader.GetDestinationIdentifier());
    EXPECT_EQ(*packHeader.GetDestinationIdentifier(), *baseHeader.GetDestinationIdentifier());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkConstructorTest)
{
    auto const optMessage = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    MessageHeader const header = optMessage->GetMessageHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Network);
    EXPECT_EQ(header.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(header.GetDestinationIdentifier());
    EXPECT_EQ(*header.GetDestinationIdentifier(), test::ServerIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkPackTest)
{
    auto const optBaseMessage = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    MessageHeader const baseHeader = optBaseMessage->GetMessageHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Network);
    EXPECT_EQ(baseHeader.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(baseHeader.GetDestinationIdentifier());
    EXPECT_EQ(*baseHeader.GetDestinationIdentifier(), test::ServerIdentifier);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = NetworkMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    MessageHeader const packHeader = optPackMessage->GetMessageHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSourceIdentifier(), baseHeader.GetSourceIdentifier());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    ASSERT_TRUE(packHeader.GetDestinationIdentifier());
    EXPECT_EQ(*packHeader.GetDestinationIdentifier(), *baseHeader.GetDestinationIdentifier());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ClusterDestinationTest)
{
    MessageContext const context = local::GenerateMessageContext();

    auto const optMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetCommand(test::Handler, test::Phase)
        .MakeClusterMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    MessageHeader const header = optMessage->GetMessageHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Cluster);
    EXPECT_FALSE(header.GetDestinationIdentifier());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkDestinationTest)
{
    MessageContext const context = local::GenerateMessageContext();

    auto const optMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetCommand(test::Handler, test::Phase)
        .MakeNetworkMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    MessageHeader const header = optMessage->GetMessageHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Network);
    EXPECT_FALSE(header.GetDestinationIdentifier());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ClusterPackTest)
{
    MessageContext const context = local::GenerateMessageContext();

    auto const optBaseMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetCommand(test::Handler, test::Phase)
        .MakeClusterMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    MessageHeader const baseHeader = optBaseMessage->GetMessageHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(baseHeader.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Cluster);
    EXPECT_FALSE(baseHeader.GetDestinationIdentifier());

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    MessageHeader const packHeader = optPackMessage->GetMessageHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSourceIdentifier(), baseHeader.GetSourceIdentifier());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    EXPECT_FALSE(packHeader.GetDestinationIdentifier());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolTest)
{
    MessageContext const context = local::GenerateMessageContext();
    
    auto const optNetworkMessage = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optNetworkMessage);

    auto const networkBuffer = Z85::Decode(optNetworkMessage->GetPack());
    auto const optNetworkProtocol = Message::PeekProtocol(networkBuffer);

    ASSERT_TRUE(optNetworkProtocol);
    EXPECT_EQ(*optNetworkProtocol, Message::Protocol::Network);

    auto const optApplicationMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Handler, test::Phase)
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
    MessageContext const context = local::GenerateMessageContext();
    
    auto const optNetworkMessage = NetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optNetworkMessage);

    auto const networkBuffer = Z85::Decode(optNetworkMessage->GetPack());
    auto const optNetworkSize = Message::PeekSize(networkBuffer);

    ASSERT_TRUE(optNetworkSize);
    EXPECT_EQ(*optNetworkSize, optNetworkMessage->GetPack().size());

    auto const optApplicationMessage = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Handler, test::Phase)
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
    auto const optMessage = NetworkMessage::Builder()
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

MessageContext local::GenerateMessageContext()
{
    MessageContext context(test::EndpointIdentifier, test::EndpointProtocol);

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
