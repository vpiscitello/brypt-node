//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/PackUtils.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Proxy.hpp"
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
Message::Context GenerateExpiredContext();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
Node::Identifier const ServerIdentifier(Node::GenerateIdentifier());

auto const ServiceProvider = std::make_shared<Node::ServiceProvider>();
auto const Proxy = Peer::Proxy::CreateInstance(ClientIdentifier, ServiceProvider);

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
    EXPECT_FALSE(optMessage->GetExtension<Message::Extension::Awaitable>());

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

    EXPECT_EQ(optPackMessage, optBaseMessage);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitableConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Request, test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    EXPECT_EQ(optRequest->GetSource(), test::ClientIdentifier);
    EXPECT_EQ(optRequest->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optRequest->GetRoute(), test::RequestRoute);

    {
        auto const optAwaitable = optRequest->GetExtension<Message::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Request);
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
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Response, test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_EQ(optResponse->GetSource(), test::ClientIdentifier);
    EXPECT_EQ(optResponse->GetDestination(), test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetRoute(), test::RequestRoute);

    {
        auto const optAwaitable = optResponse->GetExtension<Message::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), test::TrackerKey);
    }

    {
        EXPECT_EQ(optResponse->GetPayload().GetStringView(), test::Data);

        auto const pack = optResponse->GetPack();
        EXPECT_EQ(pack.size(), optResponse->GetPackSize());
    }
 
    EXPECT_NE(optRequest, optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, BoundAwaitablePackConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optBoundMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Response, test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optBoundMessage);

    auto const pack = optBoundMessage->GetPack();
    EXPECT_EQ(pack.size(), optBoundMessage->GetPackSize());

    auto const optPackMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage, optBoundMessage);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, ResponseStatusConstructorTest)
{
    Message::Context const context = local::GenerateMessageContext();

    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Response, test::TrackerKey)
        .BindExtension<Message::Extension::Status>(
            Message::Extension::Status::TooManyRequests)
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    {
        auto const optAwaitable = optResponse->GetExtension<Message::Extension::Awaitable>();
        ASSERT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), test::TrackerKey);
    }

    {
        auto const optStatus = optResponse->GetExtension<Message::Extension::Status>();
        ASSERT_TRUE(optStatus);
        EXPECT_EQ(optStatus->get().GetCode(), Message::Extension::Status::TooManyRequests);
    }

    auto const pack = optResponse->GetPack();
    EXPECT_EQ(pack.size(), optResponse->GetPackSize());

    auto const optPackMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .FromEncodedPack(pack)
        .ValidatedBuild();
    ASSERT_TRUE(optPackMessage);

    EXPECT_EQ(optPackMessage, optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, ExpiredContextBuilderTest)
{
    auto const optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(local::GenerateExpiredContext())
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Response, test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    auto const pack = optMessage->GetPack();
    EXPECT_TRUE(pack.empty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ApplicationMessageSuite, ExpiredContextPackTest)
{
    auto const optMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(local::GenerateMessageContext())
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetRoute(test::RequestRoute)
        .SetPayload(test::Data)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Response, test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    auto const optPackMessage = Message::Application::Parcel::GetBuilder()
        .SetContext(local::GenerateExpiredContext())
        .FromEncodedPack(optMessage->GetPack())
        .ValidatedBuild();
    EXPECT_FALSE(optPackMessage);

    EXPECT_NE(optPackMessage, optMessage);
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context local::GenerateMessageContext()
{
    Message::Context context(test::Proxy, test::EndpointIdentifier, test::EndpointProtocol);

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

Message::Context local::GenerateExpiredContext()
{
    Message::Context context(std::weak_ptr<Peer::Proxy>{}, test::EndpointIdentifier, test::EndpointProtocol);

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
