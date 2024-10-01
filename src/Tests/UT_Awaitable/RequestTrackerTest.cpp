//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Awaitable/Tracker.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Message/ApplicationMessage.hpp"
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
        m_spProxy = Peer::Proxy::CreateInstance(*test::ServerIdentifier, m_spServiceProvider);
        m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>(
            Awaitable::Test::EndpointIdentifier,
            Awaitable::Test::EndpointProtocol,
            Awaitable::Test::RemoteClientAddress);

        auto const optContext = m_spProxy->GetMessageContext(Awaitable::Test::EndpointIdentifier);
        ASSERT_TRUE(optContext);
        m_context = *optContext;

        auto const optRequest = Awaitable::Test::GenerateRequest(m_context, test::ClientIdentifier, *test::ServerIdentifier);
        ASSERT_TRUE(optRequest);
        m_request = *optRequest;
    }

    static std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;

    std::shared_ptr<Peer::Proxy> m_spProxy;
    Message::Context m_context;

    Message::Application::Parcel m_request;
    std::size_t m_remaining;
};

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Node::ServiceProvider> RequestTrackerSuite::m_spServiceProvider = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, SingleRequestTest)
{
    m_remaining = 1;
    
    auto const onResponse = [&] (Peer::Action::Response const& response) {
        EXPECT_EQ(response.GetTrackerKey(), Awaitable::Test::TrackerKey);
        EXPECT_EQ(response.GetSource(), *test::ServerIdentifier);
        EXPECT_EQ(response.GetPayload(), Awaitable::Test::Message);
        EXPECT_EQ(response.GetEndpointProtocol(), Awaitable::Test::EndpointProtocol);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::Ok);
        EXPECT_FALSE(response.HasErrorCode());
        EXPECT_EQ(response.GetRemaining(), --m_remaining);

        auto const& message = response.GetUnderlyingMessage<InvokeContext::Test>();
        EXPECT_EQ(message.GetSource(), response.GetSource());
        EXPECT_EQ(message.GetDestination(), test::ClientIdentifier);
        EXPECT_EQ(message.GetRoute(), Awaitable::Test::RequestRoute);
        EXPECT_EQ(message.GetPayload(), response.GetPayload());

        auto const optAwaitable = message.GetExtension<Message::Extension::Awaitable>();
        EXPECT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), response.GetTrackerKey());

        // The status code is defaulted to "Ok" when it is not explicitly set. 
        EXPECT_FALSE(message.GetExtension<Message::Extension::Status>());
    };

    auto const onError = [&] (Peer::Action::Response const&) { EXPECT_FALSE(true); };

    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, onResponse, onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);

    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Fulfilled);
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    EXPECT_EQ(m_remaining, std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, MultiRequestTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 5);
    m_remaining = identifiers.size();

    auto const onResponse = [&] (Peer::Action::Response const& response) {
        EXPECT_EQ(response.GetTrackerKey(), Awaitable::Test::TrackerKey);
        EXPECT_EQ(response.GetPayload(), Awaitable::Test::Message);
        EXPECT_EQ(response.GetEndpointProtocol(), Awaitable::Test::EndpointProtocol);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::Ok);
        EXPECT_FALSE(response.HasErrorCode());
        EXPECT_EQ(response.GetRemaining(), --m_remaining);

        auto const& message = response.GetUnderlyingMessage<InvokeContext::Test>();
        EXPECT_EQ(message.GetSource(), response.GetSource());
        EXPECT_EQ(message.GetDestination(), *test::ServerIdentifier);
        EXPECT_EQ(message.GetRoute(), Awaitable::Test::RequestRoute);
        EXPECT_EQ(message.GetPayload(), response.GetPayload());

        auto const optAwaitable = message.GetExtension<Message::Extension::Awaitable>();
        EXPECT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), response.GetTrackerKey());

        // The status code is defaulted to "Ok" when it is not explicitly set. 
        EXPECT_FALSE(message.GetExtension<Message::Extension::Status>());
    };

    auto const onError = [&] (Peer::Action::Response const&) { EXPECT_FALSE(true); };

    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, identifiers.size(), onResponse, onError };

    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.GetStatus(), Awaitable::ITracker::Status::Pending);

    for (auto const& spIdentifier : identifiers) { EXPECT_TRUE(tracker.Correlate(spIdentifier)); }
    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);

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
        EXPECT_EQ(m_remaining, identifiers.size() - updates);
    });

    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
    
    EXPECT_EQ(m_remaining, std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, DirectUpdateTest)
{
    m_remaining = 1;

    auto const onResponse = [&] (Peer::Action::Response const&) { EXPECT_FALSE(true); };
    auto const onError = [&] (Peer::Action::Response const&) { EXPECT_FALSE(true); };

    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, onResponse, onError };
    
    // Currently, requests can not be directly updated without a message.
    EXPECT_EQ(
        tracker.Update(*test::ServerIdentifier, Awaitable::Test::Message),
        Awaitable::ITracker::UpdateResult::Unexpected);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);

    EXPECT_EQ(m_remaining, std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, ExpiredRequestTest)
{
    m_remaining = 1;

    auto const onResponse = [&](Peer::Action::Response const&) { EXPECT_FALSE(true); };

    auto const onError = [&] (Peer::Action::Response const& response) {
        EXPECT_EQ(response.GetTrackerKey(), Awaitable::Test::TrackerKey);
        EXPECT_EQ(response.GetSource(), *test::ServerIdentifier);
        EXPECT_TRUE(response.GetPayload().IsEmpty());
        EXPECT_EQ(response.GetEndpointProtocol(), Network::Protocol::Invalid);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::RequestTimeout);
        EXPECT_TRUE(response.HasErrorCode());
        EXPECT_EQ(response.GetRemaining(), --m_remaining);
    };

    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, onResponse, onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
    
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);
    
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *test::ServerIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Expired);
    EXPECT_FALSE(tracker.Fulfill());

    EXPECT_EQ(m_remaining, std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, DuplicateResponseTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 2);
    m_remaining = identifiers.size();

    auto const onResponse = [&] (Peer::Action::Response const& response) {
        EXPECT_EQ(response.GetTrackerKey(), Awaitable::Test::TrackerKey);
        EXPECT_EQ(response.GetPayload(), Awaitable::Test::Message);
        EXPECT_EQ(response.GetEndpointProtocol(), Awaitable::Test::EndpointProtocol);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::Ok);
        EXPECT_FALSE(response.HasErrorCode());
        EXPECT_EQ(response.GetRemaining(), --m_remaining);

        auto const& message = response.GetUnderlyingMessage<InvokeContext::Test>();
        EXPECT_EQ(message.GetSource(), response.GetSource());
        EXPECT_EQ(message.GetDestination(), *test::ServerIdentifier);
        EXPECT_EQ(message.GetRoute(), Awaitable::Test::RequestRoute);
        EXPECT_EQ(message.GetPayload(), response.GetPayload());

        auto const optAwaitable = message.GetExtension<Message::Extension::Awaitable>();
        EXPECT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), response.GetTrackerKey());

        // The status code is defaulted to "Ok" when it is not explicitly set. 
        EXPECT_FALSE(message.GetExtension<Message::Extension::Status>());
    };

    auto const onError = [&] (Peer::Action::Response const&) { EXPECT_FALSE(true); };

    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, identifiers.size(), onResponse, onError };

    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.GetStatus(), Awaitable::ITracker::Status::Pending);

    for (auto const& spIdentifier : identifiers) { EXPECT_TRUE(tracker.Correlate(spIdentifier)); }
    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);

    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *identifiers.front(), *test::ServerIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);

    {
        auto updated = *optResponse;
        EXPECT_EQ(tracker.Update(std::move(updated)), Awaitable::ITracker::UpdateResult::Partial);
    }

    {
        auto updated = *optResponse;
        EXPECT_EQ(tracker.Update(std::move(updated)), Awaitable::ITracker::UpdateResult::Unexpected);
    }

    EXPECT_TRUE(tracker.Fulfill());
    
    EXPECT_EQ(m_remaining, std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, PartialExpiredRequestTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 16);
    m_remaining = identifiers.size();

    auto const onResponse = [&] (Peer::Action::Response const& response) {
        EXPECT_EQ(response.GetTrackerKey(), Awaitable::Test::TrackerKey);
        EXPECT_EQ(response.GetPayload(), Awaitable::Test::Message);
        EXPECT_EQ(response.GetEndpointProtocol(), Awaitable::Test::EndpointProtocol);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::Ok);
        EXPECT_FALSE(response.HasErrorCode());
        EXPECT_EQ(response.GetRemaining(), --m_remaining);

        auto const& message = response.GetUnderlyingMessage<InvokeContext::Test>();
        EXPECT_EQ(message.GetSource(), response.GetSource());
        EXPECT_EQ(message.GetDestination(), *test::ServerIdentifier);
        EXPECT_EQ(message.GetRoute(), Awaitable::Test::RequestRoute);
        EXPECT_EQ(message.GetPayload(), response.GetPayload());

        auto const optAwaitable = message.GetExtension<Message::Extension::Awaitable>();
        EXPECT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), response.GetTrackerKey());

        // The status code is defaulted to "Ok" when it is not explicitly set. 
        EXPECT_FALSE(message.GetExtension<Message::Extension::Status>());
    };

    auto const onError = [&] (Peer::Action::Response const& response) { 
        EXPECT_EQ(response.GetTrackerKey(), Awaitable::Test::TrackerKey);
        EXPECT_EQ(response.GetEndpointProtocol(), Network::Protocol::Invalid);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::RequestTimeout);
        EXPECT_TRUE(response.HasErrorCode());
        EXPECT_EQ(response.GetRemaining(), --m_remaining);
    };

    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, identifiers.size(), onResponse, onError };

    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.GetStatus(), Awaitable::ITracker::Status::Pending);

    for (auto const& spIdentifier : identifiers) { EXPECT_TRUE(tracker.Correlate(spIdentifier)); }
    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);

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
        
        EXPECT_EQ(m_remaining, identifiers.size() - responded);
    });

    EXPECT_EQ(tracker.GetReceived(), responded);
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
    EXPECT_EQ(tracker.GetReceived(), identifiers.size());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, ResponsesWithStatusCodesTest)
{
    auto const onResponse = [&](Peer::Action::Response const& response) {
        EXPECT_EQ(response.GetTrackerKey(), Awaitable::Test::TrackerKey);
        EXPECT_EQ(response.GetPayload(), Awaitable::Test::Message);
        EXPECT_EQ(response.GetEndpointProtocol(), Awaitable::Test::EndpointProtocol);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::Accepted);
        EXPECT_FALSE(response.HasErrorCode());
        EXPECT_EQ(response.GetRemaining(), --m_remaining);

        auto const& message = response.GetUnderlyingMessage<InvokeContext::Test>();
        EXPECT_EQ(message.GetSource(), response.GetSource());
        EXPECT_EQ(message.GetDestination(), *test::ServerIdentifier);
        EXPECT_EQ(message.GetRoute(), Awaitable::Test::RequestRoute);
        EXPECT_EQ(message.GetPayload(), response.GetPayload());

        auto const optAwaitable = message.GetExtension<Message::Extension::Awaitable>();
        EXPECT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), response.GetTrackerKey());

        auto const optStatus = message.GetExtension<Message::Extension::Status>();
        EXPECT_TRUE(optStatus);
        EXPECT_EQ(optStatus->get().GetCode(), response.GetStatusCode());
    };

    auto const onError = [&](Peer::Action::Response const& response) {
        EXPECT_EQ(response.GetTrackerKey(), Awaitable::Test::TrackerKey);
        EXPECT_EQ(response.GetPayload(), Awaitable::Test::Message);
        EXPECT_EQ(response.GetEndpointProtocol(), Awaitable::Test::EndpointProtocol);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::BadRequest);
        EXPECT_TRUE(response.HasErrorCode());
        EXPECT_EQ(response.GetRemaining(), --m_remaining);

        auto const& message = response.GetUnderlyingMessage<InvokeContext::Test>();
        EXPECT_EQ(message.GetSource(), response.GetSource());
        EXPECT_EQ(message.GetDestination(), *test::ServerIdentifier);
        EXPECT_EQ(message.GetRoute(), Awaitable::Test::RequestRoute);
        EXPECT_EQ(message.GetPayload(), response.GetPayload());

        auto const optAwaitable = message.GetExtension<Message::Extension::Awaitable>();
        EXPECT_TRUE(optAwaitable);
        EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Response);
        EXPECT_EQ(optAwaitable->get().GetTracker(), response.GetTrackerKey());

        auto const optStatus = message.GetExtension<Message::Extension::Status>();
        EXPECT_TRUE(optStatus);
        EXPECT_EQ(optStatus->get().GetCode(), response.GetStatusCode());
    };

    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 16);
    m_remaining = identifiers.size();

    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, identifiers.size(), onResponse, onError };

    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_EQ(tracker.GetStatus(), Awaitable::ITracker::Status::Pending);

    for (auto const& spIdentifier : identifiers) { EXPECT_TRUE(tracker.Correlate(spIdentifier)); }
    EXPECT_EQ(tracker.GetExpected(), identifiers.size());
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);

    std::random_device device;
    std::mt19937 generator{ device() };
    std::vector<Node::SharedIdentifier> successCodeIdentifiers;
    std::ranges::sample(identifiers, std::back_inserter(successCodeIdentifiers), 8, generator);
    
    std::vector<Node::SharedIdentifier> errorCodeIdentifiers;
    std::ranges::for_each(identifiers, [&] (auto const& spIdentifier) {
        auto const itr = std::ranges::find_if(successCodeIdentifiers, [&spIdentifier] (auto const& spOther) {
            return spIdentifier == spOther;
        });
        if (itr == successCodeIdentifiers.end()) {
            errorCodeIdentifiers.emplace_back(spIdentifier);
        }
    });

    std::size_t responded = 0;
    std::ranges::for_each(successCodeIdentifiers, [&] (Node::SharedIdentifier const& spIdentifier) {
        auto optResponse = Message::Application::Parcel::GetBuilder()
            .SetContext(m_context)
            .SetSource(*spIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(Awaitable::Test::RequestRoute)
            .SetPayload(Awaitable::Test::Message)
            .BindExtension<Message::Extension::Awaitable>(
                Message::Extension::Awaitable::Binding::Response, Awaitable::Test::TrackerKey)
            .BindExtension<Message::Extension::Status>(
                Message::Extension::Status::Accepted)
            .ValidatedBuild();
        ASSERT_TRUE(optResponse);
        
        auto const status = tracker.Update(std::move(*optResponse));
        auto const expected = (responded + 1 < identifiers.size()) ?
            Awaitable::ITracker::UpdateResult::Partial : Awaitable::ITracker::UpdateResult::Fulfilled;
        EXPECT_EQ(status, expected);

        EXPECT_TRUE(tracker.Fulfill());
        ++responded;
    });

    EXPECT_EQ(tracker.GetReceived(), responded);

    std::ranges::for_each(errorCodeIdentifiers, [&] (Node::SharedIdentifier const& spIdentifier) {
        auto optResponse = Message::Application::Parcel::GetBuilder()
            .SetContext(m_context)
            .SetSource(*spIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(Awaitable::Test::RequestRoute)
            .SetPayload(Awaitable::Test::Message)
            .BindExtension<Message::Extension::Awaitable>(
                Message::Extension::Awaitable::Binding::Response, Awaitable::Test::TrackerKey)
            .BindExtension<Message::Extension::Status>(
                Message::Extension::Status::BadRequest)
            .ValidatedBuild();
        ASSERT_TRUE(optResponse);
        
        auto const status = tracker.Update(std::move(*optResponse));
        auto const expected = (responded + 1 < identifiers.size()) ?
            Awaitable::ITracker::UpdateResult::Partial : Awaitable::ITracker::UpdateResult::Fulfilled;
        EXPECT_EQ(status, expected);

        EXPECT_TRUE(tracker.Fulfill());
        ++responded;
    });

    EXPECT_EQ(tracker.GetReceived(), responded);
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Completed);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(RequestTrackerSuite, UnexpectedResponseTest)
{
    auto const onResponse = [&](Peer::Action::Response const&) { EXPECT_FALSE(true); };
    auto const onError = [&](Peer::Action::Response const&) { EXPECT_FALSE(true); };

    Awaitable::RequestTracker tracker{ Awaitable::Test::TrackerKey, m_spProxy, onResponse, onError };

    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);

    auto const spIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
    auto optResponse = Awaitable::Test::GenerateResponse(
        m_context, *spIdentifier, test::ClientIdentifier, Awaitable::Test::RequestRoute, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(std::move(*optResponse)), Awaitable::ITracker::UpdateResult::Unexpected);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Pending);
}

//----------------------------------------------------------------------------------------------------------------------
