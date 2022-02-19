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

Message::Context GenerateMessageContext();

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
    Message::Context const context = local::GenerateMessageContext();

    auto const optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
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
    EXPECT_FALSE(optMessage->GetExtension<Message::Application::Extension::Awaitable>());

    auto const buffer = optMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);

    auto const pack = optMessage->GetPack();
    EXPECT_EQ(pack.size(), optMessage->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, PackConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optBaseMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .ValidatedBuild();

    auto const pack = optBaseMessage->GetPack();
    EXPECT_EQ(pack.size(), optBaseMessage->GetPackSize());

    auto const optPackMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBaseMessage->GetSourceIdentifier());
    ASSERT_TRUE(optPackMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetDestinationIdentifier(), optBaseMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetRoute(), optBaseMessage->GetRoute());
    EXPECT_EQ(optPackMessage->GetPayload(), optBaseMessage->GetPayload());
    EXPECT_FALSE(optPackMessage->GetExtension<Message::Application::Extension::Awaitable>());

    auto const buffer = optPackMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();
    Awaitable::TrackerKey const tracker = 0x89ABCDEF;

    auto const optRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Request, tracker)
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    EXPECT_EQ(optRequest->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optRequest->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optRequest->GetRoute(), test::RequestRoute);

    {
        auto const optAwaitable = optRequest->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Application::Extension::Awaitable::Request);
        EXPECT_EQ(optAwaitable->get().GetTracker(), tracker);
    }

    auto const requestBuffer = optRequest->GetPayload();
    std::string const requestData(requestBuffer.begin(), requestBuffer.end());
    EXPECT_EQ(requestData, test::Data);

    auto const requestPack = optRequest->GetPack();
    EXPECT_EQ(requestPack.size(), optRequest->GetPackSize());

    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Response, tracker)
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_EQ(optResponse->GetSourceIdentifier(), test::ClientIdentifier);
    EXPECT_EQ(optResponse->GetDestinationIdentifier(), test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetRoute(), test::RequestRoute);

    {
        auto const optAwaitable = optResponse->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Application::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), tracker);
    }

    auto const responseBuffer = optResponse->GetPayload();
    std::string const responseData(responseBuffer.begin(), responseBuffer.end());
    EXPECT_EQ(responseData, test::Data);

    auto const responsePack = optResponse->GetPack();
    EXPECT_EQ(responsePack.size(), optResponse->GetPackSize());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitPackConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();
    Awaitable::TrackerKey const tracker = 0x89ABCDEF;

    auto const optBoundMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Response, tracker)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_EQ(pack.size(), optBoundMessage->GetPackSize());

    auto const optPackMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSourceIdentifier(), optBoundMessage->GetSourceIdentifier());
    EXPECT_EQ(optPackMessage->GetDestinationIdentifier(), optBoundMessage->GetDestinationIdentifier());
    EXPECT_EQ(optPackMessage->GetRoute(), optBoundMessage->GetRoute());
    EXPECT_EQ(optPackMessage->GetPayload(), optBoundMessage->GetPayload());

    {
        auto const optBoundAwaitable = optBoundMessage->GetExtension<Message::Application::Extension::Awaitable>();
        auto const optPackAwaitable = optPackMessage->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optBoundAwaitable && optPackAwaitable);
        EXPECT_EQ(optPackAwaitable->get().GetBinding(), optPackAwaitable->get().GetBinding());
        EXPECT_EQ(optPackAwaitable->get().GetTracker(), optPackAwaitable->get().GetTracker());
    }
    
    auto const buffer = optPackMessage->GetPayload();
    std::string const data(buffer.begin(), buffer.end());
    EXPECT_EQ(data, test::Data);
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context local::GenerateMessageContext()
{
    Message::Context context(test::EndpointIdentifier, test::EndpointProtocol);

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
