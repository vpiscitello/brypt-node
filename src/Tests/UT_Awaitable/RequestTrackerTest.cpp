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
#include <random>
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
    }

    static void TearDownTestSuite()
    {
        m_spServiceProvider.reset();
    }

    void SetUp() override
    {
        m_responses = {};
        m_spProxy = Peer::Proxy::CreateInstance(*test::ServerIdentifier, m_spServiceProvider);
        m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>(
            Awaitable::Test::EndpointIdentifier,
            Awaitable::Test::EndpointProtocol,
            Awaitable::Test::RemoteClientAddress);

        auto const optContext = m_spProxy->GetMessageContext(Awaitable::Test::EndpointIdentifier);
        ASSERT_TRUE(optContext);
        m_context = *optContext;

        m_onResponse = [this] (auto const&, auto const& response) {
            m_responses.emplace_back(response);
        };
        m_onError = [this] (auto const&, auto const&, auto const& error) {
            m_optError = error;
        };

        auto const optRequest = Awaitable::Test::GenerateRequest(m_context, test::ClientIdentifier, *test::ServerIdentifier);
        ASSERT_TRUE(optRequest);
        m_request = *optRequest;
    }

    static std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;

    std::shared_ptr<Peer::Proxy> m_spProxy;
    Message::Context m_context;

    Peer::Action::OnResponse m_onResponse;
    Peer::Action::OnError m_onError;
    Message::Application::Parcel m_request;
    std::vector<Message::Application::Parcel> m_responses;
    std::optional<Peer::Action::Error> m_optError;
};

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Node::ServiceProvider> RequestTrackerSuite::m_spServiceProvider = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, SingleRequestTest)
{
    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_onResponse, m_onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_TRUE(m_responses.empty());
    EXPECT_FALSE(m_optError);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);

    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Fulfilled);
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    EXPECT_EQ(m_responses.size(), std::size_t{ 1 });
    EXPECT_EQ(m_responses.front().GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_responses.front().GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_responses.front().GetRoute(), Awaitable::Test::RequestRoute);

    auto const optExtension = m_responses.front().GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, MultiRequestTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 5);
    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, identifiers.size(), m_onResponse, m_onError };

    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.GetStatus(), Awaitable::ITracker::Status::Pending);

    for (auto const& spIdentifier : identifiers) { EXPECT_TRUE(tracker.Correlate(spIdentifier)); }
    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_TRUE(m_responses.empty());

    std::ranges::for_each(identifiers, [&, updates = std::size_t{ 0 }] (auto const& spIdentifier) mutable {
        ++updates;

        auto optResponse = Awaitable::Test::GenerateResponse(
            m_context, *spIdentifier, *test::ServerIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
        ASSERT_TRUE(optResponse);
        auto const status = tracker.Update(std::move(*optResponse));
        {
            auto const expected = (updates < identifiers.size()) ?
                Awaitable::ITracker::UpdateResult::Partial : Awaitable::ITracker::UpdateResult::Fulfilled;
            EXPECT_EQ(status, expected);
        }

        {
            auto const expected = (updates < identifiers.size()) ? 
              Awaitable::ITracker::Status::Pending : Awaitable::ITracker::Status::Fulfilled;
            EXPECT_EQ(tracker.CheckStatus(), expected);
        }
        
        EXPECT_TRUE(tracker.Fulfill());
    });

    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    EXPECT_EQ(m_responses.size(), identifiers.size());
    for (auto const& response : m_responses) {
        EXPECT_EQ(response.GetDestination(), *test::ServerIdentifier);
        EXPECT_EQ(response.GetRoute(), Awaitable::Test::RequestRoute);
        
        auto const optExtension = response.GetExtension<Message::Application::Extension::Awaitable>();
        EXPECT_TRUE(optExtension);
        EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
        EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, DirectUpdateTest)
{
    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_onResponse, m_onError };
    EXPECT_EQ(
        tracker.Update(*test::ServerIdentifier, Awaitable::Test::Message),
        Awaitable::ITracker::UpdateResult::Unexpected);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_TRUE(m_responses.empty());
    EXPECT_FALSE(m_optError);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, ExpiredRequestTest)
{
    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_onResponse, m_onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_TRUE(m_responses.empty());
    EXPECT_FALSE(m_optError);
    
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);
    
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
    EXPECT_TRUE(m_responses.empty());
    ASSERT_TRUE(m_optError);
    EXPECT_EQ(*m_optError, Peer::Action::Error::Expired);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Expired);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_TRUE(m_responses.empty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, PartialExpiredRequestTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 16);
    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, identifiers.size(), m_onResponse, m_onError };

    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.GetStatus(), Awaitable::ITracker::Status::Pending);

    for (auto const& spIdentifier : identifiers) { EXPECT_TRUE(tracker.Correlate(spIdentifier)); }
    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_TRUE(m_responses.empty());

    std::random_device device;
    std::mt19937 generator{ device() };
    std::vector<Node::SharedIdentifier> sample;
    std::ranges::sample(identifiers, std::back_inserter(sample), 8, generator);

    std::size_t responded = 0;
    std::ranges::for_each(sample, [&] (auto const& spIdentifier) {
        auto optResponse = Awaitable::Test::GenerateResponse(
            m_context, *spIdentifier, *test::ServerIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
        ASSERT_TRUE(optResponse);
        auto const status = tracker.Update(std::move(*optResponse));
        EXPECT_EQ(status, Awaitable::ITracker::UpdateResult::Partial);
        EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
        EXPECT_TRUE(tracker.Fulfill());
        ++responded;
    });

    EXPECT_EQ(tracker.GetReceived(), responded);
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
    EXPECT_EQ(tracker.GetReceived(), identifiers.size());

    EXPECT_EQ(m_responses.size(), responded);
    for (auto const& response : m_responses) {
        EXPECT_EQ(response.GetDestination(), *test::ServerIdentifier);
        EXPECT_EQ(response.GetRoute(), Awaitable::Test::RequestRoute);
        
        auto const optExtension = response.GetExtension<Message::Application::Extension::Awaitable>();
        EXPECT_TRUE(optExtension);
        EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
        EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, UnexpectedResponseTest)
{
    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, m_onResponse, m_onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_TRUE(m_responses.empty());
    EXPECT_FALSE(m_optError);

    {
        auto const spIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
        auto optResponse = Awaitable::Test::GenerateResponse(
            m_context, *spIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
        ASSERT_TRUE(optResponse);
        EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Unexpected);
        EXPECT_FALSE(tracker.Fulfill());
        EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
        EXPECT_TRUE(m_responses.empty());
        EXPECT_FALSE(m_optError);
    }

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Fulfilled);
    
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    EXPECT_EQ(m_responses.size(), std::size_t{ 1 });
    EXPECT_EQ(m_responses.front().GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_responses.front().GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_responses.front().GetRoute(), Awaitable::Test::RequestRoute);

    auto const optExtension = m_responses.front().GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);
}

//----------------------------------------------------------------------------------------------------------------------
