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

class RequestTrackerSuite : public testing::Test
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
        m_spProxy = Peer::Proxy::CreateInstance(*test::ServerIdentifier, m_spServiceProvider);
        m_onResponse = [this] (auto const& response) {
            m_optFulfilledResponse = response;
        };
        m_onError = [this] (auto const&, auto const& error) {
            m_optError = error;
        };

        auto const optRequest = Awaitable::Test::GenerateRequest(m_context, test::ClientIdentifier, *test::ServerIdentifier);
        ASSERT_TRUE(optRequest);
        m_request = *optRequest;
    }

    static std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    static Message::Context m_context;

    std::shared_ptr<Peer::Proxy> m_spProxy;
    Peer::Action::OnResponse m_onResponse;
    Peer::Action::OnError m_onError;
    Message::Application::Parcel m_request;
    std::optional<Message::Application::Parcel> m_optFulfilledResponse;
    std::optional<Peer::Action::Error> m_optError;
};

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Node::ServiceProvider> RequestTrackerSuite::m_spServiceProvider = {};
Message::Context RequestTrackerSuite::m_context = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, FulfilledRequestTest)
{
    Awaitable::RequestTracker tracker{ m_spProxy, m_onResponse, m_onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(m_optFulfilledResponse);
    EXPECT_FALSE(m_optError);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);

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

TEST_F(RequestTrackerSuite, DirectUpdateTest)
{
    Awaitable::RequestTracker tracker{ m_spProxy, m_onResponse, m_onError };
    EXPECT_EQ(
        tracker.Update(*test::ServerIdentifier, Awaitable::Test::Message),
        Awaitable::ITracker::UpdateResult::Unexpected);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(m_optFulfilledResponse);
    EXPECT_FALSE(m_optError);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, ExpiredRequestTest)
{
    Awaitable::RequestTracker tracker{ m_spProxy, m_onResponse, m_onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(m_optFulfilledResponse);
    EXPECT_FALSE(m_optError);
    
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);
    
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
    EXPECT_FALSE(m_optFulfilledResponse);
    ASSERT_TRUE(m_optError);
    EXPECT_EQ(*m_optError, Peer::Action::Error::Expired);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Expired);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_FALSE(m_optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, UnexpectedResponseTest)
{
    Awaitable::RequestTracker tracker{ m_spProxy, m_onResponse, m_onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(m_optFulfilledResponse);
    EXPECT_FALSE(m_optError);

    {
        auto const spIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
        auto optResponse = Awaitable::Test::GenerateResponse(
            m_context, *spIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
        ASSERT_TRUE(optResponse);
        EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Unexpected);
        EXPECT_FALSE(tracker.Fulfill());
        EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
        EXPECT_FALSE(m_optFulfilledResponse);
        EXPECT_FALSE(m_optError);
    }

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
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
