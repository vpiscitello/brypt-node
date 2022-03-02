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

class AggregateTrackerSuite : public testing::Test
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
        m_spClientPeer = Peer::Proxy::CreateInstance(Awaitable::Test::ClientIdentifier, m_spServiceProvider);
        m_spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
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

        auto const optRequest = Awaitable::Test::GenerateRequest(m_context);
        ASSERT_TRUE(optRequest);
        m_request = *optRequest;
    }

    static std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    static Message::Context m_context;
    std::shared_ptr<Peer::Proxy> m_spClientPeer;
    Message::Application::Parcel m_request;
    std::optional<Message::Application::Parcel> m_optFulfilledResponse;
};

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Node::ServiceProvider> AggregateTrackerSuite::m_spServiceProvider = {};
Message::Context AggregateTrackerSuite::m_context = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AggregateTrackerSuite, SingleResponseTest)
{
    Awaitable::AggregateTracker tracker{ m_spClientPeer, m_request, { Awaitable::Test::spServerIdentifier } };
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Unfulfilled);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_FALSE(m_optFulfilledResponse);

    auto const optResponse = Awaitable::Test::GenerateResponse(
        m_context,  Awaitable::Test::spServerIdentifier, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(*optResponse), Awaitable::ITracker::UpdateResult::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());

    ASSERT_TRUE(m_optFulfilledResponse);
    EXPECT_EQ(m_optFulfilledResponse->GetSource(), *Awaitable::Test::spServerIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetDestination(), Awaitable::Test::ClientIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);

    auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AggregateTrackerSuite, MultipleResponseTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(3);
    Awaitable::AggregateTracker tracker{ m_spClientPeer, m_request, identifiers };
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Unfulfilled);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_FALSE(m_optFulfilledResponse);

    std::ranges::for_each(identifiers, [&, updates = std::size_t{ 0 }] (auto const& spIdentifier) mutable {
        auto const optResponse = Awaitable::Test::GenerateResponse(
            m_context,  spIdentifier, Awaitable::Test::TrackerKey);
        ASSERT_TRUE(optResponse);
        auto const status = tracker.Update(*optResponse);
        auto const expected = (++updates < identifiers.size()) ?
            Awaitable::ITracker::UpdateResult::Success : Awaitable::ITracker::UpdateResult::Fulfilled;
        EXPECT_EQ(status, expected);
    });

    EXPECT_TRUE(tracker.Fulfill());
    ASSERT_TRUE(m_optFulfilledResponse);
    EXPECT_EQ(m_optFulfilledResponse->GetSource(), *Awaitable::Test::spServerIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetDestination(), Awaitable::Test::ClientIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);
    
    auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AggregateTrackerSuite, ExpiredNoResponsesTest)
{
    Awaitable::AggregateTracker tracker { m_spClientPeer, m_request, { Awaitable::Test::spServerIdentifier } };
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_TRUE(tracker.Fulfill());

    ASSERT_TRUE(m_optFulfilledResponse);
    EXPECT_EQ(m_optFulfilledResponse->GetSource(), *Awaitable::Test::spServerIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetDestination(), Awaitable::Test::ClientIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);
    EXPECT_GT(m_optFulfilledResponse->GetPayload().size(), std::size_t{ 0 });

    auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AggregateTrackerSuite, ExpiredSomeResponsesTest)
{
    auto const identifiers = Awaitable::Test::GenerateIdentifiers(3);
    Awaitable::AggregateTracker tracker{ m_spClientPeer, m_request, identifiers };
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Unfulfilled);
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_FALSE(m_optFulfilledResponse);
    
    auto const matches = [updates = std::size_t{ 0 }] (auto const&) mutable { return ++updates != 1; };
    std::ranges::for_each(identifiers| std::views::filter(matches), [&] (auto const& spIdentifier) mutable {
        auto const optResponse = Awaitable::Test::GenerateResponse(
            m_context,  spIdentifier, Awaitable::Test::TrackerKey);
        ASSERT_TRUE(optResponse);
        auto const status = tracker.Update(*optResponse);
        EXPECT_EQ(status, Awaitable::ITracker::UpdateResult::Success);
    });
    
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);
    EXPECT_TRUE(tracker.Fulfill());

    ASSERT_TRUE(m_optFulfilledResponse);
    EXPECT_EQ(m_optFulfilledResponse->GetSource(), *Awaitable::Test::spServerIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetDestination(), Awaitable::Test::ClientIdentifier);
    EXPECT_EQ(m_optFulfilledResponse->GetRoute(), Awaitable::Test::RequestRoute);
    EXPECT_GT(m_optFulfilledResponse->GetPayload().size(), std::size_t{ 0 });

    auto const optExtension = m_optFulfilledResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Awaitable::Test::TrackerKey);  
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AggregateTrackerSuite, ExpiredLateResponsesTest)
{
    Awaitable::AggregateTracker tracker{ m_spClientPeer, m_request, { Awaitable::Test::spServerIdentifier } };
    std::this_thread::sleep_for(Awaitable::AggregateTracker::ExpirationPeriod + 1ms);
    EXPECT_EQ(tracker.CheckStatus(), Awaitable::ITracker::Status::Fulfilled);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_TRUE(tracker.Fulfill());
    EXPECT_TRUE(m_optFulfilledResponse);

    auto const optResponse = Awaitable::Test::GenerateResponse(
        m_context,  Awaitable::Test::spServerIdentifier, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);    
    EXPECT_EQ(tracker.Update(*optResponse), Awaitable::ITracker::UpdateResult::Expired);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AggregateTrackerSuite, UnexpectedResponsesTest)
{
    Awaitable::AggregateTracker tracker{ m_spClientPeer, m_request, { Awaitable::Test::spServerIdentifier } };
    auto const spUnexpectedIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
    auto const optResponse = Awaitable::Test::GenerateResponse(
        m_context,  spUnexpectedIdentifier, Awaitable::Test::TrackerKey);
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(tracker.Update(*optResponse), Awaitable::ITracker::UpdateResult::Unexpected);
    EXPECT_EQ(tracker.GetReceived(), std::size_t{ 0 });
    EXPECT_FALSE(tracker.Fulfill());
    EXPECT_FALSE(m_optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------
