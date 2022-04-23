//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Awaitable/Tracker.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------
using namespace std::chrono_literals;
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const ServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
Node::Identifier const ClientIdentifier{ Node::GenerateIdentifier() };

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class DeferredTrackerSuite : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();
        m_context = Awaitable::Test::GenerateMessageContext();
    }

    static void TearDownTestSuite()
    {
        m_spServiceProvider.reset();
        m_context = {};
    }

    void SetUp() override
    {
        m_optFulfilledResponse = {};
        m_spProxy = Peer::Proxy::CreateInstance(test::ClientIdentifier, m_spServiceProvider);
        m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>(
            Awaitable::Test::EndpointIdentifier,
            Awaitable::Test::EndpointProtocol,
            Awaitable::Test::RemoteClientAddress,
            [this] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                auto const optMessage = Message::Application::Parcel::GetBuilder()
                    .SetContext(m_context)
                    .FromEncodedPack(std::get<std::string>(message))
                    .ValidatedBuild();
                EXPECT_TRUE(optMessage);

                Message::ValidationStatus status = optMessage->Validate();
                if (status != Message::ValidationStatus::Success) { return false; }
                m_optFulfilledResponse = optMessage;
                return true;
            });

        auto const optRequest = Awaitable::Test::GenerateRequest(m_context, test::ClientIdentifier, *test::ServerIdentifier);
        ASSERT_TRUE(optRequest);
        m_request = *optRequest;
    }

    static std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    static Message::Context m_context;

    std::shared_ptr<Peer::Proxy> m_spProxy;
    Message::Application::Parcel m_request;
    std::optional<Message::Application::Parcel> m_optFulfilledResponse;
};

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Node::ServiceProvider> DeferredTrackerSuite::m_spServiceProvider = {};
Message::Context DeferredTrackerSuite::m_context = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(DeferredTrackerSuite, SingleResponseTest)
{
    Awaitable::DeferredTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_request, { test::ServerIdentifier } };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(m_optFulfilledResponse);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, *test::ServerIdentifier, Awaitable::Test::NoticeRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Fulfilled);

    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    ASSERT_TRUE(m_optFulfilledResponse);
    EXPECT_EQ(m_optFulfilledResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);

    auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(DeferredTrackerSuite, MultipleResponseTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 3);
    Awaitable::DeferredTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_request, identifiers };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(m_optFulfilledResponse);

    std::ranges::for_each(identifiers, [&, updates = std::size_t{ 0 }] (auto const& spIdentifier) mutable {
        auto optResponse = Awaitable::Test::GenerateResponse(
            m_context, *spIdentifier, *test::ServerIdentifier, Awaitable::Test::NoticeRoute, Awaitable::Test::TrackerKey);
        ASSERT_TRUE(optResponse);
        auto const status = tracker.Update(std::move(*optResponse));
        auto const expected = (++updates < identifiers.size()) ?
            Awaitable::ITracker::UpdateResult::Success : Awaitable::ITracker::UpdateResult::Fulfilled;
        EXPECT_EQ(status, expected);
    });

    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    ASSERT_TRUE(m_optFulfilledResponse);
    EXPECT_EQ(m_optFulfilledResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);
    
    auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(DeferredTrackerSuite, DirectUpdateTest)
{
    Awaitable::DeferredTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_request, { test::ServerIdentifier } };

    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_EQ(
        tracker.Update(*test::ServerIdentifier, Awaitable::Test::Message),
        Awaitable::ITracker::UpdateResult::Fulfilled);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 1 });
    
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
    
    ASSERT_TRUE(m_optFulfilledResponse);
    EXPECT_EQ(m_optFulfilledResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);
    EXPECT_FALSE(m_optFulfilledResponse->GetPayload().IsEmpty());

    auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(DeferredTrackerSuite, ExpiredNoResponsesTest)
{
    Awaitable::DeferredTracker tracker { Awaitable::Test::TrackerKey, m_spProxy, m_request, { test::ServerIdentifier } };
    // std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

    // EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    // EXPECT_TRUE(tracker.Fulfill());
    // EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    // ASSERT_TRUE(m_optFulfilledResponse);
    // EXPECT_EQ(m_optFulfilledResponse->GetSource(), *test::ServerIdentifier);
    // EXPECT_EQ(m_optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    // EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);
    // EXPECT_FALSE(m_optFulfilledResponse->GetPayload().IsEmpty());

    // auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    // EXPECT_TRUE(optExtension);
    // EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    // EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(DeferredTrackerSuite, ExpiredSomeResponsesTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 3);
    Awaitable::DeferredTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_request, identifiers };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(m_optFulfilledResponse);
    
    auto const matches = [updates = std::size_t{ 0 }] (auto const&) mutable { return ++updates != 1; };
    std::ranges::for_each(identifiers| std::views::filter(matches), [&] (auto const& spIdentifier) mutable {
        auto optResponse = Awaitable::Test::GenerateResponse(
            m_context, *spIdentifier, *test::ServerIdentifier, Awaitable::Test::NoticeRoute, Awaitable::Test::TrackerKey);
        ASSERT_TRUE(optResponse);
        auto const status = tracker.Update(std::move(*optResponse));
        EXPECT_EQ(status, Awaitable::ITracker::UpdateResult::Success);
    });
    
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    ASSERT_TRUE(m_optFulfilledResponse);
    EXPECT_EQ(m_optFulfilledResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);
    EXPECT_FALSE(m_optFulfilledResponse->GetPayload().IsEmpty());

    auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(DeferredTrackerSuite, ExpiredLateResponsesTest)
{
    Awaitable::DeferredTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_request, { test::ServerIdentifier } };
    std::this_thread::sleep_for(Awaitable::DeferredTracker::ExpirationPeriod + 1ms);

    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
    EXPECT_TRUE(m_optFulfilledResponse);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, *test::ServerIdentifier, Awaitable::Test::NoticeRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);    
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Expired);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(DeferredTrackerSuite, UnexpectedResponsesTest)
{
    Awaitable::DeferredTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_request, { test::ServerIdentifier } };
    Node::Identifier const unexpected{ Node::GenerateIdentifier() };
    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, unexpected, *test::ServerIdentifier, Awaitable::Test::NoticeRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Unexpected);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(m_optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------
