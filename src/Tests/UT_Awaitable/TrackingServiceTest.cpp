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
#include "Components/Scheduler/TaskService.hpp"
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

class TrackingServiceSuite : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        m_spScheduler = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();
        m_spTaskService = std::make_shared<Scheduler::TaskService>(m_spScheduler);
        m_spServiceProvider->Register(m_spTaskService);
        m_context = Awaitable::Test::GenerateMessageContext();
    }

    static void TearDownTestSuite()
    {
        m_spTaskService.reset();
        m_spServiceProvider.reset();
        m_spScheduler.reset();
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

    static std::shared_ptr<Scheduler::Registrar> m_spScheduler;
    static std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    static std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    static Message::Context m_context;

    std::shared_ptr<Peer::Proxy> m_spProxy;
    Message::Application::Parcel m_request;
    std::optional<Message::Application::Parcel> m_optFulfilledResponse;
};

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Scheduler::Registrar> TrackingServiceSuite::m_spScheduler = {};
std::shared_ptr<Node::ServiceProvider> TrackingServiceSuite::m_spServiceProvider = {};
std::shared_ptr<Scheduler::TaskService> TrackingServiceSuite::m_spTaskService = {};
Message::Context TrackingServiceSuite::m_context = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(TrackingServiceSuite, DeferredRequestFulfilledTest)
{
    Awaitable::TrackingService service{ m_spScheduler, m_spServiceProvider };
    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });

    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 3);
    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*test::ServerIdentifier)
        .SetRoute(Awaitable::Test::NoticeRoute)
        .MakeClusterMessage();

    // Stage the deferred request such that other "nodes" can be notified and response.    
    auto const optTrackerKey = service.StageDeferred(m_spProxy, identifiers, m_request, builder);
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
    EXPECT_EQ(service.Waiting(), std::size_t{ 1 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });

    for (auto const& spIdentifier : identifiers) {
        auto optResponse = Awaitable::Test::GenerateResponse(
            m_context, *spIdentifier, *test::ServerIdentifier, Awaitable::Test::NoticeRoute, *optTrackerKey);
        ASSERT_TRUE(optResponse);
        EXPECT_TRUE(service.Process(std::move(*optResponse)));
    }

    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 1 });
    
    EXPECT_EQ(service.Execute(), std::size_t{ 1 }); // The service should indicate one awaitable was fulfilled. 
    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });
    EXPECT_TRUE(m_optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(TrackingServiceSuite, ExpiredAwaitableTest)
{
    Awaitable::TrackingService service{ m_spScheduler, m_spServiceProvider };
    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });

    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*test::ServerIdentifier)
        .SetRoute(Awaitable::Test::NoticeRoute)
        .MakeClusterMessage();

    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 3);
    auto const optTrackerKey = service.StageDeferred(m_spProxy, identifiers, m_request, builder);
    ASSERT_TRUE(optTrackerKey);
    EXPECT_NE(*optTrackerKey, Awaitable::TrackerKey{});

    EXPECT_EQ(service.Execute(), std::size_t{ 0 });  
    EXPECT_EQ(service.Waiting(), std::size_t{ 1 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });
    
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

     // The service needs to check for expired awaitables before they can be fulfilled. 
    EXPECT_EQ(service.Execute(), std::size_t{ 0 }); 
    EXPECT_EQ(service.Waiting(), std::size_t{ 1 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });

    service.CheckTrackers();
    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 1 });

    EXPECT_EQ(service.Execute(), std::size_t{ 1 }); 
    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });

    EXPECT_TRUE(m_optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------