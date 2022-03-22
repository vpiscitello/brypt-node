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
#include "Interfaces/SecurityStrategy.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
#include <lithium_json.hh>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <memory>
#include <thread>
#include <unordered_set>
//----------------------------------------------------------------------------------------------------------------------
using namespace std::chrono_literals;
//----------------------------------------------------------------------------------------------------------------------

#ifndef LI_SYMBOL_identifier
#define LI_SYMBOL_identifier
LI_SYMBOL(identifier)
#endif
#ifndef LI_SYMBOL_data
#define LI_SYMBOL_data
LI_SYMBOL(data)
#endif

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = Node::Identifier{ Node::GenerateIdentifier() };
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class PeerActionSuite : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        m_context = Peer::Test::GenerateMessageContext();

        auto const optMessage = Message::Application::Parcel::GetBuilder()
            .SetContext(m_context)
            .SetSource(test::ClientIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(Peer::Test::RequestRoute)
            .SetPayload(Peer::Test::RequestPayload)
            .BindExtension<Message::Application::Extension::Awaitable>(
                Message::Application::Extension::Awaitable::Request, Peer::Test::TrackerKey)
            .ValidatedBuild();
        ASSERT_TRUE(optMessage);
        m_message = *optMessage;
    }

    static void TearDownTestSuite()
    {
        m_message = {};
        m_context = {};
    }

    void SetUp() override
    {
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

        m_spTaskService = std::make_shared<Scheduler::TaskService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTaskService);

        m_spTrackingService = std::make_shared<Awaitable::TrackingService>(m_spRegistrar, m_spServiceProvider);
        m_spServiceProvider->Register(m_spTrackingService);

        m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
        m_spServiceProvider->Register(m_spEventPublisher);

        m_spNodeState = std::make_shared<NodeState>(test::ServerIdentifier, Network::ProtocolSet{});
        m_spServiceProvider->Register(m_spNodeState);

        m_spConnectProtocol = std::make_shared<Peer::Test::ConnectProtocol>();
        m_spServiceProvider->Register<IConnectProtocol>(m_spConnectProtocol);

        m_spMessageProcessor = std::make_shared<Peer::Test::MessageProcessor>();
        m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);
        
        m_spMediator = std::make_shared<Peer::Test::PeerMediator>();
        m_spServiceProvider->Register<IPeerMediator>(m_spMediator);

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
    }

    static Message::Context m_context;
    static Message::Application::Parcel m_message;

    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Peer::Test::ConnectProtocol> m_spConnectProtocol;
    std::shared_ptr<Peer::Test::MessageProcessor> m_spMessageProcessor;
    std::shared_ptr<Peer::Test::PeerMediator> m_spMediator;
    std::shared_ptr<Peer::Test::PeerCache> m_spCache;
    std::shared_ptr<Peer::Proxy> m_spProxy;

    std::optional<Message::Application::Parcel> m_optResult;
};

//----------------------------------------------------------------------------------------------------------------------

Message::Context PeerActionSuite::m_context = {};
Message::Application::Parcel PeerActionSuite::m_message = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, FulfilledRequestTest)
{
    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*test::ServerIdentifier)
        .SetRoute(Peer::Test::RequestRoute)
        .SetPayload(Peer::Test::RequestPayload);
        
    std::optional<Message::Application::Parcel> optResponse;
    std::optional<Peer::Action::Error> optError;
    
    bool const result = m_spProxy->Request(builder, 
        [&optResponse] (auto const& response) { optResponse = response; },
        [&optError] (auto const& error) { optError = error; });
    EXPECT_TRUE(result);
    EXPECT_FALSE(optResponse);
    EXPECT_FALSE(optError);

    ASSERT_TRUE(m_optResult);
    EXPECT_EQ(m_optResult->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResult->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optResult->GetRoute(), Peer::Test::RequestRoute);
    EXPECT_EQ(m_optResult->GetPayload(), Peer::Test::RequestPayload);
    
    auto const optRequestExtension = m_optResult->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optRequestExtension);
    EXPECT_EQ(optRequestExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Request);
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
            .BindExtension<Message::Application::Extension::Awaitable>(
                Message::Application::Extension::Awaitable::Binding::Response,
                optRequestExtension->get().GetTracker())
            .ValidatedBuild();
        ASSERT_TRUE(optMessage);
            
        EXPECT_TRUE(m_spTrackingService->Process(std::move(*optMessage)));
        EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 0 });
        EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 1 });
    }

    EXPECT_EQ(m_spTrackingService->Execute(), std::size_t{ 1 });
    EXPECT_FALSE(optError);

    ASSERT_TRUE(optResponse);
    EXPECT_EQ(optResponse->GetSource(), test::ClientIdentifier);
    EXPECT_EQ(optResponse->GetDestination(), *test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetRoute(), Peer::Test::RequestRoute);
    EXPECT_EQ(optResponse->GetPayload(), Peer::Test::ApplicationPayload);

    auto const optResponseExtension = optResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optResponseExtension);
    EXPECT_EQ(optResponseExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    ASSERT_EQ(optResponseExtension->get().GetTracker(), optRequestExtension->get().GetTracker());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, ExpiredRequestTest)
{
    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*test::ServerIdentifier)
        .SetRoute(Peer::Test::RequestRoute)
        .SetPayload(Peer::Test::RequestPayload);
        
    std::optional<Message::Application::Parcel> optResponse;
    std::optional<Peer::Action::Error> optError;
    
    bool const result = m_spProxy->Request(builder, 
        [&optResponse] (auto const& response) { optResponse = response; },
        [&optError] (auto const& error) { optError = error; });
    EXPECT_TRUE(result);
    EXPECT_FALSE(optResponse);
    EXPECT_FALSE(optError);

    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 1 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 0 });
    std::this_thread::sleep_for(Awaitable::ITracker::ExpirationPeriod + 1ms);

    m_spTrackingService->CheckTrackers();
    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 0 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 1 });

    EXPECT_EQ(m_spTrackingService->Execute(), std::size_t{ 1 });
    EXPECT_EQ(optError, Peer::Action::Error::Expired);
    EXPECT_FALSE(optResponse);
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
    
    auto const optExtension = m_optResult->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Peer::Test::TrackerKey);

    {
        auto const json = m_optResult->GetPayload().GetStringView();
        struct PayloadEntry {
            std::string identifier;
            std::vector<std::uint8_t> data;
        };
        std::vector<PayloadEntry> deserialized;
        auto const error = li::json_object_vector(s::identifier, s::data).decode(json, deserialized);
        EXPECT_FALSE(error.code);
        EXPECT_FALSE(deserialized.empty());

        std::unordered_set<Node::Identifier, Node::IdentifierHasher> identifiers;
        bool const isExpectedPayload = std::ranges::all_of(deserialized, [&] (PayloadEntry const& entry) -> bool {
            Node::Identifier identifier{ entry.identifier };
            if (!identifier.IsValid()) { return false; }
            if (auto const& [itr, emplaced] = identifiers.emplace(identifier); !emplaced) { return false; }
            auto const buffer = std::string_view{ reinterpret_cast<char const*>(entry.data.data()), entry.data.size() };
            return buffer == Peer::Test::ApplicationPayload;
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
    
    ASSERT_TRUE(m_optResult);
    EXPECT_EQ(m_optResult->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResult->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optResult->GetRoute(), Peer::Test::ResponseRoute);
    EXPECT_EQ(m_optResult->GetPayload().GetStringView(), Peer::Test::ApplicationPayload);
    EXPECT_FALSE(m_optResult->GetExtension<Message::Application::Extension::Awaitable>());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerActionSuite, RespondTest)
{
    Peer::Action::Next next{ m_spProxy, m_message, m_spServiceProvider };
    EXPECT_EQ(next.GetProxy().lock(), m_spProxy);

    EXPECT_TRUE(next.Respond(Peer::Test::ApplicationPayload));

    ASSERT_TRUE(m_optResult);
    EXPECT_EQ(m_optResult->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResult->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(m_optResult->GetRoute(), Peer::Test::RequestRoute);
    EXPECT_EQ(m_optResult->GetPayload().GetStringView(), Peer::Test::ApplicationPayload);

    auto const optExtension = m_optResult->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optExtension);
    EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
    EXPECT_EQ(optExtension->get().GetTracker(), Peer::Test::TrackerKey);  
}

//----------------------------------------------------------------------------------------------------------------------
