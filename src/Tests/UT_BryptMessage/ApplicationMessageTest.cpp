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

constexpr Network::Endpoint::Identifier EndpointIdentifier = 1;
constexpr Network::Protocol EndpointProtocol = Network::Protocol::TCP;

constexpr Awaitable::TrackerKey TrackerKey = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
};

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BaseConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    EXPECT_EQ(optMessage->GetSource(), test::ClientIdentifier);
    ASSERT_TRUE(optMessage->GetDestination());
    EXPECT_EQ(*optMessage->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optMessage->GetRoute(), test::RequestRoute);
    EXPECT_FALSE(optMessage->GetExtension<Message::Application::Extension::Awaitable>());

    {
        EXPECT_EQ(optMessage->GetPayload().GetStringView(), test::Data);

        auto const pack = optMessage->GetPack();
        EXPECT_EQ(pack.size(), optMessage->GetPackSize());
    }

    {
        auto const payload = optMessage->ExtractPayload();
        EXPECT_EQ(payload.GetStringView(), test::Data);
        EXPECT_TRUE(optMessage->GetPayload().GetReadableView().empty());
    }
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

    EXPECT_EQ(optPackMessage->GetSource(), optBaseMessage->GetSource());
    ASSERT_TRUE(optPackMessage->GetDestination());
    EXPECT_EQ(optPackMessage->GetDestination(), optBaseMessage->GetDestination());
    EXPECT_EQ(optPackMessage->GetRoute(), optBaseMessage->GetRoute());
    EXPECT_EQ(optPackMessage->GetPayload(), optBaseMessage->GetPayload());
    EXPECT_EQ(optPackMessage->GetPayload().GetStringView(), test::Data);
    EXPECT_FALSE(optPackMessage->GetExtension<Message::Application::Extension::Awaitable>());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Request, test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    EXPECT_EQ(optRequest->GetSource(), test::ClientIdentifier);
    EXPECT_EQ(optRequest->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optRequest->GetRoute(), test::RequestRoute);

    {
        auto const optAwaitable = optRequest->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Application::Extension::Awaitable::Request);
        EXPECT_EQ(optAwaitable->get().GetTracker(), test::TrackerKey);
    }

    {
        EXPECT_EQ(optRequest->GetPayload().GetStringView(), test::Data);

        auto const pack = optRequest->GetPack();
        EXPECT_EQ(pack.size(), optRequest->GetPackSize());
    }

    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Response, test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_EQ(optResponse->GetSource(), test::ClientIdentifier);
    EXPECT_EQ(optResponse->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetRoute(), test::RequestRoute);

    {
        auto const optAwaitable = optResponse->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Application::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), test::TrackerKey);
    }

    {
        EXPECT_EQ(optResponse->GetPayload().GetStringView(), test::Data);

        auto const pack = optResponse->GetPack();
        EXPECT_EQ(pack.size(), optResponse->GetPackSize());
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitPackConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optBoundMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Response, test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_EQ(pack.size(), optBoundMessage->GetPackSize());

    auto const optPackMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage->GetSource(), optBoundMessage->GetSource());
    EXPECT_EQ(optPackMessage->GetDestination(), optBoundMessage->GetDestination());
    EXPECT_EQ(optPackMessage->GetRoute(), optBoundMessage->GetRoute());
    EXPECT_EQ(optPackMessage->GetPayload(), optBoundMessage->GetPayload());
    EXPECT_EQ(optPackMessage->GetPayload().GetStringView(), test::Data);

    {
        auto const optBoundAwaitable = optBoundMessage->GetExtension<Message::Application::Extension::Awaitable>();
        auto const optPackAwaitable = optPackMessage->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optBoundAwaitable && optPackAwaitable);
        EXPECT_EQ(optPackAwaitable->get().GetBinding(), optPackAwaitable->get().GetBinding());
        EXPECT_EQ(optPackAwaitable->get().GetTracker(), optPackAwaitable->get().GetTracker());
    }
    
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
