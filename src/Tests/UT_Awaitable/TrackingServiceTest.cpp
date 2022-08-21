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
#include <random>
#include <thread>
#include <unordered_set>
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
        EXPECT_TRUE(m_spScheduler->Initialize());
    }

    static void TearDownTestSuite()
    {
        m_spTaskService.reset();
        m_spServiceProvider.reset();
        m_spScheduler.reset();
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

        auto const optContext = m_spProxy->GetMessageContext(Awaitable::Test::EndpointIdentifier);
        ASSERT_TRUE(optContext);
        m_context = *optContext;

        auto const optRequest = Awaitable::Test::GenerateRequest(m_context, test::ClientIdentifier, *test::ServerIdentifier);
        ASSERT_TRUE(optRequest);
        m_request = *optRequest;
    }

    static std::shared_ptr<Scheduler::Registrar> m_spScheduler;
    static std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    static std::shared_ptr<Scheduler::TaskService> m_spTaskService;

    std::shared_ptr<Peer::Proxy> m_spProxy;
    Message::Context m_context;
    Message::Application::Parcel m_request;
    std::optional<Message::Application::Parcel> m_optFulfilledResponse;
};

//----------------------------------------------------------------------------------------------------------------------

std::shared_ptr<Scheduler::Registrar> TrackingServiceSuite::m_spScheduler = {};
std::shared_ptr<Node::ServiceProvider> TrackingServiceSuite::m_spServiceProvider = {};
std::shared_ptr<Scheduler::TaskService> TrackingServiceSuite::m_spTaskService = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(TrackingServiceSuite, DeferredFulfillmentTest)
{
    Awaitable::TrackingService service{ m_spScheduler };
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
        auto const optExtension = optNotice->GetExtension<Message::Extension::Awaitable>();
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
    Awaitable::TrackingService service{ m_spScheduler };
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

    auto const frames = Scheduler::Frame{ Awaitable::TrackingService::CheckInterval.GetValue() };
    EXPECT_EQ(m_spScheduler->Run<InvokeContext::Test>(frames), std::size_t{ 1 });
    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });

    EXPECT_TRUE(m_optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(TrackingServiceSuite, RequestFulfillmentTest)
{
    Awaitable::TrackingService service{ m_spScheduler };
    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });

    std::unordered_set<Node::Identifier, Node::IdentifierHasher> processed;

    auto const onResponse = [&processed] (Peer::Action::Response const& response) {
        auto const [itr, emplaced] = processed.emplace(response.GetSource());
        EXPECT_TRUE(emplaced);
        EXPECT_EQ(response.GetPayload(), Awaitable::Test::Message);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::Ok);
    };

    auto const onError = [&processed] (Peer::Action::Response const& response) {
        auto const [itr, emplaced] = processed.emplace(response.GetSource());
        EXPECT_TRUE(emplaced);
        EXPECT_TRUE(response.GetPayload().IsEmpty());
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::RequestTimeout);
    };

    auto const identifiers = Awaitable::Test::GenerateIdentifiers(test::ServerIdentifier, 16);

    // Stage the deferred request such that other "nodes" can be notified and response.    
    auto const optResult = service.StageRequest(*test::ServerIdentifier, identifiers.size(), onResponse, onError);
    ASSERT_TRUE(optResult); // The service should supply a tracker key on success. 

    auto const& [tracker, correlator] = *optResult;
    EXPECT_NE(tracker, Awaitable::TrackerKey{}); // The key should not be defaulted.
    for (auto const& spIdentifier : identifiers) { EXPECT_TRUE(correlator(spIdentifier)); }

    std::random_device device;
    std::mt19937 generator{ device() };
    std::vector<Node::SharedIdentifier> sample;
    std::ranges::sample(identifiers, std::back_inserter(sample), 8, generator);

    std::size_t responded = 0;
    std::ranges::for_each(sample, [&] (auto const& spIdentifier) {
        auto optResponse = Awaitable::Test::GenerateResponse(
            m_context, *spIdentifier, *test::ServerIdentifier, Awaitable::Test::RequestRoute, tracker);
        ASSERT_TRUE(optResponse);
        EXPECT_TRUE(service.Process(std::move(*optResponse)));
        EXPECT_EQ(service.Waiting(), std::size_t{ 1 });
        EXPECT_EQ(service.Ready(), std::size_t{ 1 }); // The responses should be sent to the handler as they are received.
        EXPECT_EQ(service.Execute(), std::size_t{ 1 });
        ++responded;
    });

    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

    auto const frames = Scheduler::Frame{ Awaitable::TrackingService::CheckInterval.GetValue() };
    EXPECT_EQ(m_spScheduler->Run<InvokeContext::Test>(frames), std::size_t{ 1 });
    EXPECT_EQ(service.Waiting(), std::size_t{ 0 });
    EXPECT_EQ(service.Ready(), std::size_t{ 0 });

    for (auto const& spIdentifier : identifiers) { EXPECT_TRUE(processed.contains(*spIdentifier)); }
}

//----------------------------------------------------------------------------------------------------------------------
