//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/Definitions.hpp"
#include "Components/Awaitable/Tracker.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------
using namespace std::chrono_literals;
//----------------------------------------------------------------------------------------------------------------------

class TrackingServiceSuite : public testing::Test
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
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
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
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar = std::make_shared<Scheduler::Registrar>();
    std::shared_ptr<Peer::Proxy> m_spClientPeer;
    Message::Application::Parcel m_request;
    std::optional<Message::Application::Parcel> m_optFulfilledResponse;
};

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Node::ServiceProvider> TrackingServiceSuite::m_spServiceProvider = {};
Message::Context TrackingServiceSuite::m_context = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(TrackingServiceSuite, DeferredRequestFulfilledTest)
{
    Awaitable::TrackingService service{ m_spRegistrar };

    auto const identifiers = Awaitable::Test::GenerateIdentifiers(3);
    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*Awaitable::Test::spServerIdentifier)
        .SetRoute(Awaitable::Test::NoticeRoute)
        .MakeClusterMessage();

    // Stage the deferred request such that other "nodes" can be notified and response.    
    auto const optTrackerKey = service.StageDeferred(m_spClientPeer, identifiers, m_request, builder);
    ASSERT_TRUE(optTrackerKey); // The service should supply a tracker key on success. 
    EXPECT_NE(*optTrackerKey, Awaitable::TrackerKey{}); // The key should not be defaulted.

    {
        auto const optNotice = builder.ValidatedBuild(); 
        ASSERT_TRUE(optNotice); // The notice builder should succeed. 

        // The notice should have an awaitable extension applied that associates it with the deferred request/
        auto const optExtension = optNotice->GetExtension<Message::Application::Extension::Awaitable>();
        ASSERT_TRUE(optExtension);
        EXPECT_EQ(*optTrackerKey, optExtension->get().GetTracker());
    }

    EXPECT_EQ(service.Execute(), std::size_t{ 0 });  

    for (auto const& spIdentifier : identifiers) {
        auto const optResponse = Awaitable::Test::GenerateResponse(m_context, spIdentifier, *optTrackerKey);
        ASSERT_TRUE(optResponse);
        EXPECT_TRUE(service.Process(*optResponse));
    }

    EXPECT_EQ(service.Execute(), std::size_t{ 1 }); // The service should indicate one awaitable was fulfilled. 
    EXPECT_TRUE(m_optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(TrackingServiceSuite, ExpiredAwaitableTest)
{
    Awaitable::TrackingService service{ m_spRegistrar };

    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*Awaitable::Test::spServerIdentifier)
        .SetRoute(Awaitable::Test::NoticeRoute)
        .MakeClusterMessage();

    auto const identifiers = Awaitable::Test::GenerateIdentifiers(3);
    auto const optTrackerKey = service.StageDeferred(m_spClientPeer, identifiers, m_request, builder);
    ASSERT_TRUE(optTrackerKey);
    EXPECT_NE(*optTrackerKey, Awaitable::TrackerKey{});

    EXPECT_EQ(service.Execute(), std::size_t{ 0 });  

    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

    // The service should indicate one awaitable was fulfilled even if it expires with no responses.
    EXPECT_EQ(service.Execute(), std::size_t{ 1 });
    EXPECT_TRUE(m_optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------
