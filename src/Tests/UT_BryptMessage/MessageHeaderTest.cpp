//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptIdentifier/IdentifierDefinitions.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/NetworkMessage.hpp"
#include "../../BryptMessage/MessageUtils.hpp"
#include "../../Utilities/Z85.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
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
constexpr std::uint8_t Phase = 0;

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ApplicationConstructorTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    auto const optMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::Phase)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    CMessageHeader const header = optMessage->GetMessageHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(header.GetDestinationIdentifier());
    EXPECT_EQ(*header.GetDestinationIdentifier(), test::ServerIdentifier);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ApplicationPackTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::Phase)
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    CMessageHeader const baseHeader = optBaseMessage->GetMessageHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(baseHeader.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(baseHeader.GetDestinationIdentifier());
    EXPECT_EQ(*baseHeader.GetDestinationIdentifier(), test::ServerIdentifier);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    CMessageHeader const packHeader = optPackMessage->GetMessageHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSourceIdentifier(), baseHeader.GetSourceIdentifier());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    ASSERT_TRUE(packHeader.GetDestinationIdentifier());
    EXPECT_EQ(*packHeader.GetDestinationIdentifier(), *baseHeader.GetDestinationIdentifier());
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkConstructorTest)
{
    auto const optMessage = CNetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    CMessageHeader const header = optMessage->GetMessageHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Network);
    EXPECT_EQ(header.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(header.GetDestinationIdentifier());
    EXPECT_EQ(*header.GetDestinationIdentifier(), test::ServerIdentifier);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkPackTest)
{
    auto const optBaseMessage = CNetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    CMessageHeader const baseHeader = optBaseMessage->GetMessageHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Network);
    EXPECT_EQ(baseHeader.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Node);
    ASSERT_TRUE(baseHeader.GetDestinationIdentifier());
    EXPECT_EQ(*baseHeader.GetDestinationIdentifier(), test::ServerIdentifier);

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = CNetworkMessage::Builder()
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    CMessageHeader const packHeader = optPackMessage->GetMessageHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSourceIdentifier(), baseHeader.GetSourceIdentifier());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    ASSERT_TRUE(packHeader.GetDestinationIdentifier());
    EXPECT_EQ(*packHeader.GetDestinationIdentifier(), *baseHeader.GetDestinationIdentifier());
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ClusterDestinationTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    auto const optMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetCommand(test::Command, test::Phase)
        .MakeClusterMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    CMessageHeader const header = optMessage->GetMessageHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Cluster);
    EXPECT_FALSE(header.GetDestinationIdentifier());
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, NetworkDestinationTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    auto const optMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetCommand(test::Command, test::Phase)
        .MakeNetworkMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    CMessageHeader const header = optMessage->GetMessageHeader();
    EXPECT_EQ(header.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(header.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(header.GetDestinationType(), Message::Destination::Network);
    EXPECT_FALSE(header.GetDestinationIdentifier());
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, ClusterPackTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    auto const optBaseMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetCommand(test::Command, test::Phase)
        .MakeClusterMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optBaseMessage);

    CMessageHeader const baseHeader = optBaseMessage->GetMessageHeader();
    EXPECT_EQ(baseHeader.GetMessageProtocol(), Message::Protocol::Application);
    EXPECT_EQ(baseHeader.GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(baseHeader.GetDestinationType(), Message::Destination::Cluster);
    EXPECT_FALSE(baseHeader.GetDestinationIdentifier());

    auto const pack = optBaseMessage->GetPack();

    auto const optPackMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    CMessageHeader const packHeader = optPackMessage->GetMessageHeader();
    EXPECT_EQ(packHeader.GetMessageProtocol(), baseHeader.GetMessageProtocol());
    EXPECT_EQ(packHeader.GetSourceIdentifier(), baseHeader.GetSourceIdentifier());
    EXPECT_EQ(packHeader.GetDestinationType(), baseHeader.GetDestinationType());
    EXPECT_FALSE(packHeader.GetDestinationIdentifier());
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolTest)
{
    CMessageContext const context = local::GenerateMessageContext();
    
    auto const optNetworkMessage = CNetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optNetworkMessage);

    auto const networkBuffer = Z85::Decode(optNetworkMessage->GetPack());
    auto const optNetworkProtocol = Message::PeekProtocol(networkBuffer);

    ASSERT_TRUE(optNetworkProtocol);
    EXPECT_EQ(*optNetworkProtocol, Message::Protocol::Network);

    auto const optApplicationMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::Phase)
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationMessage);

    auto const applicationBuffer = Z85::Decode(optApplicationMessage->GetPack());
    auto const optApplicationProtocol = Message::PeekProtocol(applicationBuffer);

    ASSERT_TRUE(optApplicationProtocol);
    EXPECT_EQ(*optApplicationProtocol, Message::Protocol::Application);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolNullBytesTest)
{
    Message::Buffer const buffer(12, 0x00);
    auto const optProtocol = Message::PeekProtocol(buffer);
    EXPECT_FALSE(optProtocol);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolOutOfRangeBytesTest)
{
    Message::Buffer const buffer(12, 0xF0);
    auto const optProtocol = Message::PeekProtocol(buffer);
    EXPECT_FALSE(optProtocol);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekProtocolEmptyBufferTest)
{
    Message::Buffer const buffer;
    auto const optProtocol = Message::PeekProtocol(buffer);
    EXPECT_FALSE(optProtocol);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSizeTest)
{
    CMessageContext const context = local::GenerateMessageContext();
    
    auto const optNetworkMessage = CNetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optNetworkMessage);

    auto const networkBuffer = Z85::Decode(optNetworkMessage->GetPack());
    auto const optNetworkSize = Message::PeekSize(networkBuffer);

    ASSERT_TRUE(optNetworkSize);
    EXPECT_EQ(*optNetworkSize, optNetworkMessage->GetPack().size());

    auto const optApplicationMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::Phase)
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationMessage);

    auto const applicationBuffer = Z85::Decode(optApplicationMessage->GetPack());
    auto const optApplicationSize = Message::PeekSize(applicationBuffer);

    ASSERT_TRUE(optApplicationSize);
    EXPECT_EQ(*optApplicationSize, optApplicationMessage->GetPack().size());
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSizeNullBytesTest)
{
    Message::Buffer const buffer(12, 0x00);
    auto const optMessageSize = Message::PeekSize(buffer);
    EXPECT_FALSE(optMessageSize);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSizeEmptyBufferTest)
{
    Message::Buffer const buffer;
    auto const optMessageSize = Message::PeekSize(buffer);
    EXPECT_FALSE(optMessageSize);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceTest)
{
    auto const optMessage = CNetworkMessage::Builder()
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

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceNullBytesTest)
{
    Message::Buffer const buffer(128, 0x00);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceInvalidIdentifierTest)
{
    Message::Buffer const buffer(128, BryptIdentifier::Network::MinimumLength);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceSmallBufferTest)
{
    Message::Buffer const buffer(12, BryptIdentifier::Network::MinimumLength);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceSmallIdentifierSizeTest)
{
    Message::Buffer const buffer(128, BryptIdentifier::Network::MaximumLength + 1);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceLargeIdentifierSizeTest)
{
    Message::Buffer const buffer(128, BryptIdentifier::Network::MinimumLength - 1);
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(MessageHeaderSuite, PeekSourceEmptyBufferTest)
{
    Message::Buffer const buffer;
    auto const optSource = Message::PeekSource(buffer);
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

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
