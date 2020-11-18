//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptIdentifier/IdentifierDefinitions.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/NetworkMessage.hpp"
#include "../../BryptMessage/MessageUtils.hpp"
#include "../../BryptMessage/PackUtils.hpp"
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

TEST(CMessageHeaderSuite, ApplicationConstructorTest)
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

TEST(CMessageHeaderSuite, ApplicationPackTest)
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

TEST(CMessageHeaderSuite, NetworkConstructorTest)
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

TEST(CMessageHeaderSuite, NetworkPackTest)
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

TEST(CMessageHeaderSuite, ClusterDestinationTest)
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

TEST(CMessageHeaderSuite, NetworkDestinationTest)
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

TEST(CMessageHeaderSuite, ClusterPackTest)
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

TEST(CMessageHeaderSuite, PeekProtocolTest)
{
    CMessageContext const context = local::GenerateMessageContext();
    
    auto const optNetworkMessage = CNetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optNetworkMessage);

    auto const networkBuffer = PackUtils::Z85Decode(optNetworkMessage->GetPack());
    auto const optNetworkProtocol = Message::PeekProtocol(
        networkBuffer.begin(), networkBuffer.end());

    ASSERT_TRUE(optNetworkProtocol);
    EXPECT_EQ(*optNetworkProtocol, Message::Protocol::Network);

    auto const optApplicationMessage = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::Phase)
        .ValidatedBuild();
    ASSERT_TRUE(optApplicationMessage);

    auto const applicationBuffer = PackUtils::Z85Decode(optApplicationMessage->GetPack());
    auto const optApplicationProtocol = Message::PeekProtocol(
        applicationBuffer.begin(), applicationBuffer.end());

    ASSERT_TRUE(optApplicationProtocol);
    EXPECT_EQ(*optApplicationProtocol, Message::Protocol::Application);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekProtocolNullBytesTest)
{
    Message::Buffer const buffer(12, 0x00);
    auto const optProtocol = Message::PeekProtocol(buffer.begin(), buffer.end());
    EXPECT_FALSE(optProtocol);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekProtocolOutOfRangeBytesTest)
{
    Message::Buffer const buffer(12, 0xF0);
    auto const optProtocol = Message::PeekProtocol(buffer.begin(), buffer.end());
    EXPECT_FALSE(optProtocol);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekProtocolEmptyBufferTest)
{
    Message::Buffer const buffer;
    auto const optProtocol = Message::PeekProtocol(buffer.begin(), buffer.end());
    EXPECT_FALSE(optProtocol);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekSourceTest)
{
    auto const optMessage = CNetworkMessage::Builder()
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    auto const buffer = PackUtils::Z85Decode(optMessage->GetPack());
    auto const optSource = Message::PeekSource(buffer.begin(), buffer.end());

    ASSERT_TRUE(optSource);
    EXPECT_EQ(*optSource, test::ClientIdentifier);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekSourceNullBytesTest)
{
    Message::Buffer const buffer(128, 0x00);
    auto const optSource = Message::PeekSource(buffer.begin(), buffer.end());
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekSourceInvalidIdentifierTest)
{
    Message::Buffer const buffer(128, BryptIdentifier::Network::MinimumLength);
    auto const optSource = Message::PeekSource(buffer.begin(), buffer.end());
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekSourceSmallBufferTest)
{
    Message::Buffer const buffer(12, BryptIdentifier::Network::MinimumLength);
    auto const optSource = Message::PeekSource(buffer.begin(), buffer.end());
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekSourceSmallIdentifierSizeTest)
{
    Message::Buffer const buffer(128, BryptIdentifier::Network::MaximumLength + 1);
    auto const optSource = Message::PeekSource(buffer.begin(), buffer.end());
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekSourceLargeIdentifierSizeTest)
{
    Message::Buffer const buffer(128, BryptIdentifier::Network::MinimumLength - 1);
    auto const optSource = Message::PeekSource(buffer.begin(), buffer.end());
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

TEST(CMessageHeaderSuite, PeekSourceEmptyBufferTest)
{
    Message::Buffer const buffer;
    auto const optSource = Message::PeekSource(buffer.begin(), buffer.end());
    EXPECT_FALSE(optSource);
}

//------------------------------------------------------------------------------------------------

CMessageContext local::GenerateMessageContext()
{
    CMessageContext context(test::EndpointIdentifier, test::EndpointTechnology);

    context.BindEncryptionHandlers(
        [] (auto const& buffer, auto, auto) -> Security::Encryptor::result_type 
            { return buffer; },
        [] (auto const& buffer, auto, auto) -> Security::Decryptor::result_type 
            { return buffer; });

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
