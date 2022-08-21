//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Resolver.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Components/Security/PostQuantum/NISTSecurityLevelThree.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/ResolutionService.hpp"
#include "Interfaces/SecurityStrategy.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json.hpp>
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <memory>
#include <thread>
#include <unordered_set>
//----------------------------------------------------------------------------------------------------------------------
using namespace std::chrono_literals;
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = Node::Identifier{ Node::GenerateIdentifier() };
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
//----------------------------------------------------------------------------------------------------------------------
namespace symbols {
//----------------------------------------------------------------------------------------------------------------------

auto const Identifier = "identifier";
auto const Data = "data";

//----------------------------------------------------------------------------------------------------------------------
} // symbols namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class PeerActionSuite : public testing::Test
{
protected:
    void SetUp() override
    {
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

        m_spTaskService = std::make_shared<Scheduler::TaskService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTaskService);

        m_spTrackingService = std::make_shared<Awaitable::TrackingService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTrackingService);

        m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
        m_spServiceProvider->Register(m_spEventPublisher);

        m_spNodeState = std::make_shared<NodeState>(test::ServerIdentifier, Network::ProtocolSet{});
        m_spServiceProvider->Register(m_spNodeState);

        m_spConnectProtocol = std::make_shared<Peer::Test::ConnectProtocol>();
        m_spServiceProvider->Register<IConnectProtocol>(m_spConnectProtocol);

        m_spMessageProcessor = std::make_shared<Peer::Test::MessageProcessor>();
        m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);
        
        m_spResolutionService = std::make_shared<Peer::Test::ResolutionService>();
        m_spServiceProvider->Register<IResolutionService>(m_spResolutionService);

        m_spCache = std::make_shared<Peer::Test::PeerCache>(5);
        m_spServiceProvider->Register<IPeerCache>(m_spCache);

        m_spProxy = Peer::Proxy::CreateInstance(test::ClientIdentifier, m_spServiceProvider);
        m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>(
            Peer::Test::EndpointIdentifier, Peer::Test::EndpointProtocol, Peer::Test::RemoteClientAddress,
            [this] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                auto const optMessage = Message::Application::Parcel::GetBuilder()
                    .SetContext(m_context)
                    .FromEncodedPack(std::get<std::string>(message))
                    .ValidatedBuild();
                EXPECT_TRUE(optMessage);

                Message::ValidationStatus status = optMessage->Validate();
                if (status != Message::ValidationStatus::Success) { return false; }
                m_optResult = optMessage;
                return true;
            });

        auto const optContext = m_spProxy->GetMessageContext(Peer::Test::EndpointIdentifier);
        ASSERT_TRUE(optContext);
        m_context = *optContext;

        auto const optMessage = Message::Application::Parcel::GetBuilder()
            .SetContext(m_context)
            .SetSource(test::ClientIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(Peer::Test::RequestRoute)
            .SetPayload(Peer::Test::RequestPayload)
            .BindExtension<Message::Extension::Awaitable>(
                Message::Extension::Awaitable::Request, Peer::Test::TrackerKey)
            .ValidatedBuild();
        ASSERT_TRUE(optMessage);
        m_message = *optMessage;

        EXPECT_TRUE(m_spRegistrar->Initialize());
    }

    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Peer::Test::ConnectProtocol> m_spConnectProtocol;
    std::shared_ptr<Peer::Test::MessageProcessor> m_spMessageProcessor;
    std::shared_ptr<Peer::Test::ResolutionService> m_spResolutionService;
    std::shared_ptr<Peer::Test::PeerCache> m_spCache;
    std::shared_ptr<Peer::Proxy> m_spProxy;
    Message::Context m_context;
    Message::Application::Parcel m_message;

    std::optional<Message::Application::Parcel> m_optResult;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, FulfilledRequestTest)
{
    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*test::ServerIdentifier)
        .SetRoute(Peer::Test::RequestRoute)
        .SetPayload(Peer::Test::RequestPayload);
    
    auto const optTrackerKey = m_spProxy->Request(builder,
        [&] (Peer::Action::Response const& response) {
            EXPECT_EQ(response.GetSource(), test::ClientIdentifier);
            EXPECT_EQ(response.GetPayload(), Peer::Test::ApplicationPayload);
            EXPECT_EQ(response.GetEndpointProtocol(), Peer::Test::EndpointProtocol);
            EXPECT_FALSE(response.HasErrorCode());
            EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::Accepted);
        },
        [&] (Peer::Action::Response const&) { EXPECT_FALSE(true); });

    EXPECT_TRUE(optTrackerKey);

    ASSERT_TRUE(m_optResult);
    EXPECT_EQ(m_optResult->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResult->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optResult->GetRoute(), Peer::Test::RequestRoute);
    EXPECT_EQ(m_optResult->GetPayload(), Peer::Test::RequestPayload);
    
    auto const optRequestExtension = m_optResult->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optRequestExtension);
    EXPECT_EQ(optRequestExtension->get().GetBinding(), Message::Extension::Awaitable::Binding::Request);
    ASSERT_NE(optRequestExtension->get().GetTracker(), Awaitable::TrackerKey{ 0 });
    ASSERT_NE(optRequestExtension->get().GetTracker(), Peer::Test::TrackerKey);
    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 1 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 0 });

    {
        auto optMessage = Message::Application::Parcel::GetBuilder()
            .SetContext(m_context)
            .SetSource(test::ClientIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(Peer::Test::RequestRoute)
            .SetPayload(Peer::Test::ApplicationPayload)
            .BindExtension<Message::Extension::Awaitable>(
                Message::Extension::Awaitable::Binding::Response, optRequestExtension->get().GetTracker())
            .BindExtension<Message::Extension::Status>(
                Message::Extension::Status::Accepted)
            .ValidatedBuild();
        ASSERT_TRUE(optMessage);
            
        EXPECT_TRUE(m_spTrackingService->Process(std::move(*optMessage)));
        EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 0 });
        EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 1 });
    }

    EXPECT_EQ(m_spTrackingService->Execute(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, ExpiredRequestTest)
{
    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*test::ServerIdentifier)
        .SetRoute(Peer::Test::RequestRoute)
        .SetPayload(Peer::Test::RequestPayload);
    
    auto const optTrackerKey = m_spProxy->Request(builder,
        [&] (Peer::Action::Response const&) { EXPECT_FALSE(true); },
        [&] (Peer::Action::Response const& response) {
            EXPECT_EQ(response.GetSource(), test::ClientIdentifier);
            EXPECT_TRUE(response.GetPayload().IsEmpty());
            EXPECT_EQ(response.GetEndpointProtocol(), Network::Protocol::Invalid);
            EXPECT_TRUE(response.HasErrorCode());
            EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::RequestTimeout);
        });
    EXPECT_TRUE(optTrackerKey);

    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 1 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 0 });
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

    auto const frames = Scheduler::Frame{ Awaitable::TrackingService::CheckInterval.GetValue() };
    EXPECT_EQ(m_spRegistrar->Run<InvokeContext::Test>(frames), std::size_t{ 1 });
    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 0 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, DeferTest)
{
    Peer::Action::Next next{ m_spProxy, m_message, m_spServiceProvider };
    EXPECT_EQ(next.GetProxy().lock(), m_spProxy);
    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 0 });

    auto const optTrackerKey = next.Defer({
        .notice = {
            .type = Message::Destination::Cluster,
            .route = Peer::Test::NoticeRoute,
            .payload = Peer::Test::NoticePayload
        },
        .response = {
            .payload = Peer::Test::ApplicationPayload
        }
    });

    ASSERT_TRUE(optTrackerKey);
    ASSERT_NE(*optTrackerKey, Awaitable::TrackerKey{ 0 });
    ASSERT_NE(*optTrackerKey, Peer::Test::TrackerKey);
    EXPECT_EQ(optTrackerKey, next.GetTrackerKey());
    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 1 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 0 });
    EXPECT_FALSE(m_optResult);

    m_spCache->ForEach([&] (Node::SharedIdentifier const& spIdentifier) -> CallbackIteration {
        [[maybe_unused]] auto const status = m_spTrackingService->Process(
            *optTrackerKey, *spIdentifier, Peer::Test::ApplicationPayload);
        return CallbackIteration::Continue;
    }, IPeerCache::Filter::Active);

    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 0 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 1 });
    EXPECT_FALSE(m_optResult);

    EXPECT_EQ(m_spTrackingService->Execute(), std::size_t{ 1 });
    ASSERT_TRUE(m_optResult);

    EXPECT_EQ(m_optResult->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResult->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optResult->GetRoute(), Peer::Test::RequestRoute);
    
    auto const optExtension = m_optResult->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Peer::Test::TrackerKey);

    {
        boost::json::error_code error;
        auto const json = boost::json::parse(m_optResult->GetPayload().GetStringView());
        ASSERT_FALSE(error);
        ASSERT_TRUE(json.is_object());

        std::unordered_set<Node::Identifier, Node::IdentifierHasher> identifiers;
        bool const isExpectedPayload = std::ranges::all_of(json.as_object(), [&] (auto const& entry) -> bool {
            auto const& [key, value] = entry;

            {
                Node::Identifier identifier{ key.data() };
                if (!identifier.IsValid()) { return false; }
                if (auto const& [itr, emplaced] = identifiers.emplace(identifier); !emplaced) { return false; }
            }

            {
                EXPECT_TRUE(value.is_string());
                auto const& dataString = value.get_string();
                auto const buffer = std::string_view{ reinterpret_cast<char const*>(dataString.data()), dataString.size() };
                if (buffer != Peer::Test::ApplicationPayload) { return false; }
            }

            return true;
        });

        EXPECT_TRUE(isExpectedPayload);

        bool hasExpectedIdentifiers = true;
        m_spCache->ForEach([&] (Node::SharedIdentifier const& spIdentifier) -> CallbackIteration {
            hasExpectedIdentifiers = hasExpectedIdentifiers && identifiers.contains(*spIdentifier);
            return CallbackIteration::Continue;
        }, IPeerCache::Filter::Active);
        EXPECT_TRUE(hasExpectedIdentifiers);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, DispatchTest)
{
    Peer::Action::Next next{ m_spProxy, m_message, m_spServiceProvider };
    EXPECT_EQ(next.GetProxy().lock(), m_spProxy);

    EXPECT_TRUE(next.Dispatch(Peer::Test::ResponseRoute, Peer::Test::ApplicationPayload));
    EXPECT_FALSE(next.GetTrackerKey());
    
    ASSERT_TRUE(m_optResult);
    EXPECT_EQ(m_optResult->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResult->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optResult->GetRoute(), Peer::Test::ResponseRoute);
    EXPECT_EQ(m_optResult->GetPayload().GetStringView(), Peer::Test::ApplicationPayload);
    EXPECT_FALSE(m_optResult->GetExtension<Message::Extension::Awaitable>());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, RespondExpectedTest)
{
    Peer::Action::Next next{ m_spProxy, m_message, m_spServiceProvider };
    EXPECT_EQ(next.GetProxy().lock(), m_spProxy);

    EXPECT_TRUE(next.Respond(Peer::Test::ApplicationPayload, Message::Extension::Status::Created));
    EXPECT_FALSE(next.GetTrackerKey());

    ASSERT_TRUE(m_optResult);
    EXPECT_EQ(m_optResult->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResult->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optResult->GetRoute(), Peer::Test::RequestRoute);
    EXPECT_EQ(m_optResult->GetPayload().GetStringView(), Peer::Test::ApplicationPayload);

    auto const optAwaitable = m_optResult->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optAwaitable);
    EXPECT_EQ(optAwaitable->get().GetBinding(), Message::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optAwaitable->get().GetTracker(), Peer::Test::TrackerKey);

    auto const optStatus = m_optResult->GetExtension<Message::Extension::Status>();
    EXPECT_TRUE(optStatus);
    EXPECT_EQ(optStatus->get().GetCode(), Message::Extension::Status::Created);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, RespondRequestMissingAwaitableTest)
{
    auto const optRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(Peer::Test::RequestRoute)
        .SetPayload(Peer::Test::RequestPayload)
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    Peer::Action::Next next{ m_spProxy, *optRequest, m_spServiceProvider };
    EXPECT_EQ(next.GetProxy().lock(), m_spProxy);

    EXPECT_FALSE(next.Respond(Peer::Test::ApplicationPayload, Message::Extension::Status::Created));
    EXPECT_FALSE(m_optResult);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, RespondRequestInvalidAwaitableTest)
{
    auto const optRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(Peer::Test::RequestRoute)
        .SetPayload(Peer::Test::RequestPayload)
        .BindExtension<Message::Extension::Awaitable>(
            Message::Extension::Awaitable::Response, Peer::Test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    Peer::Action::Next next{ m_spProxy, *optRequest, m_spServiceProvider };
    EXPECT_EQ(next.GetProxy().lock(), m_spProxy);

    EXPECT_FALSE(next.Respond(Peer::Test::ApplicationPayload, Message::Extension::Status::Created));
    EXPECT_FALSE(m_optResult);
}

//----------------------------------------------------------------------------------------------------------------------
