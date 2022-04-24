//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Manager.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/State/NodeState.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <random>
#include <ranges>
#include <string>
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

auto const ClientIdentifier = Node::Identifier{ Node::GenerateIdentifier() };
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class PeerManagerSuite : public testing::Test
{
protected:
    void SetUp() override
    {
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

        m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
        m_spServiceProvider->Register(m_spEventPublisher);

        m_spNodeState = std::make_shared<NodeState>(test::ServerIdentifier, Network::ProtocolSet{});
        m_spServiceProvider->Register(m_spNodeState);
        
        m_spTrackingService = std::make_shared<Awaitable::TrackingService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTrackingService);

        m_spConnectProtocol = std::make_shared<Peer::Test::ConnectProtocol>();
        m_spServiceProvider->Register<IConnectProtocol>(m_spConnectProtocol);

        m_spMessageProcessor = std::make_shared<Peer::Test::MessageProcessor>();
        m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);

        m_spManager = std::make_shared<Peer::Manager>(Security::Strategy::PQNISTL3, m_spServiceProvider);
        m_spServiceProvider->Register<IPeerMediator>(m_spManager);
     
        EXPECT_TRUE(m_spRegistrar->Initialize());
    }

    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<Peer::Test::ConnectProtocol> m_spConnectProtocol;
    std::shared_ptr<Peer::Test::MessageProcessor> m_spMessageProcessor;
    std::shared_ptr<Peer::Manager> m_spManager;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, PeerDeclarationTest)
{
    EXPECT_EQ(m_spManager->ResolvingCount(), std::size_t{ 0 });
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const optRequest = m_spManager->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });
    EXPECT_EQ(m_spManager->ResolvingCount(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, DuplicatePeerDeclarationTest)
{
    EXPECT_EQ(m_spManager->ResolvingCount(), std::size_t{ 0 });
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const optRequest = m_spManager->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });
    EXPECT_EQ(m_spManager->ResolvingCount(), std::size_t{ 1 });

    auto const optCheckRequest = m_spManager->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    EXPECT_FALSE(optCheckRequest);
    EXPECT_EQ(m_spManager->ResolvingCount(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, UndeclaredPeerTest)
{
    EXPECT_EQ(m_spManager->ResolvingCount(), std::size_t{ 0 });
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const optRequest = m_spManager->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });
    EXPECT_EQ(m_spManager->ResolvingCount(), std::size_t{ 1 });

    m_spManager->RescindResolvingPeer(Peer::Test::RemoteServerAddress);
    EXPECT_EQ(m_spManager->ResolvingCount(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, DeclaredPeerLinkTest)
{
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const optRequest = m_spManager->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });

    auto const spPeerProxy = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(spPeerProxy);

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_TRUE(spPeerProxy->IsEndpointRegistered(identifier));
    EXPECT_EQ(spPeerProxy->RegisteredEndpointCount(), std::size_t{ 1 });
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, UndeclaredPeerLinkTest)
{
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const spPeerProxy = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(spPeerProxy);

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_TRUE(spPeerProxy->IsEndpointRegistered(identifier));
    EXPECT_EQ(spPeerProxy->RegisteredEndpointCount(), std::size_t{ 1 });
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, ExistingPeerLinkTest)
{
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const spCreatedPeer = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spCreatedPeer);
    {
        auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
        spCreatedPeer->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
        EXPECT_TRUE(spCreatedPeer->IsEndpointRegistered(identifier));
        EXPECT_EQ(spCreatedPeer->RegisteredEndpointCount(), std::size_t{ 1 });
        EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });
    }

    {
        auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
        auto const address = Network::RemoteAddress{ Network::Protocol::LoRa, "915:71", false };
        auto const spMergedPeer = m_spManager->LinkPeer(test::ClientIdentifier, address);
        ASSERT_EQ(spMergedPeer, spCreatedPeer);

        spMergedPeer->RegisterEndpoint(identifier, Network::Protocol::LoRa, address, {});
        EXPECT_TRUE(spMergedPeer->IsEndpointRegistered(identifier));
        EXPECT_EQ(spMergedPeer->RegisteredEndpointCount(), std::size_t{ 2 });
        EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, DuplicateEqualSharedPeerLinkTest)
{
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const spCreatedPeer = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spCreatedPeer);
    {
        auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
        spCreatedPeer->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
        EXPECT_TRUE(spCreatedPeer->IsEndpointRegistered(identifier));
        EXPECT_EQ(spCreatedPeer->RegisteredEndpointCount(), std::size_t{ 1 });
        EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });
    }

    auto const duplicated = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    {
        auto const address = Network::RemoteAddress{ Network::Protocol::LoRa, "915:71", false };
        auto const spMergedPeer = m_spManager->LinkPeer(test::ClientIdentifier, address);
        ASSERT_EQ(spMergedPeer, spCreatedPeer);

        spMergedPeer->RegisterEndpoint(duplicated, Network::Protocol::LoRa, address, {});
        EXPECT_TRUE(spMergedPeer->IsEndpointRegistered(duplicated));
        EXPECT_EQ(spMergedPeer->RegisteredEndpointCount(), std::size_t{ 2 });
        EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });
    }

    {
        auto const address = Network::RemoteAddress{ Network::Protocol::LoRa, "915:72", false };
        auto const spMergedPeer = m_spManager->LinkPeer(test::ClientIdentifier, address);
        ASSERT_EQ(spMergedPeer, spCreatedPeer);

        spMergedPeer->RegisterEndpoint(duplicated, Network::Protocol::LoRa, address, {});
        EXPECT_TRUE(spMergedPeer->IsEndpointRegistered(duplicated));
        EXPECT_EQ(spMergedPeer->RegisteredEndpointCount(), std::size_t{ 2 });
        EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, PeerSingleEndpointDisconnectTest)
{
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const spPeerProxy = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spPeerProxy);

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });

    spPeerProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, PeerMultipleEndpointDisconnectTest)
{
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });

    auto const spPeerProxy = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spPeerProxy);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    auto const address = Network::RemoteAddress{ Network::Protocol::LoRa, "915:71", false };
    EXPECT_TRUE(m_spManager->LinkPeer(test::ClientIdentifier, address));
    spPeerProxy->RegisterEndpoint(loraIdentifier, Network::Protocol::LoRa, address, {});
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });

    spPeerProxy->WithdrawEndpoint(tcpIdentifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });

    spPeerProxy->WithdrawEndpoint(loraIdentifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, SingleForEachIdentifierCacheTest)
{
    auto const spPeerProxy = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 1 });

    m_spManager->ForEach([&] (Node::SharedIdentifier const& spCheckIdentifier) -> CallbackIteration {
        EXPECT_EQ(spCheckIdentifier, spPeerProxy->GetIdentifier());
        EXPECT_EQ(*spCheckIdentifier, *spPeerProxy->GetIdentifier());
        return CallbackIteration::Continue;
    });

    spPeerProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);

    std::uint32_t iterations = 0;
    EXPECT_EQ(m_spManager->ActiveCount(), std::size_t{ 0 });
    m_spManager->ForEach([&] ([[maybe_unused]] Node::SharedIdentifier const& spCheckIdentifier) -> CallbackIteration {
        ++iterations;
        return CallbackIteration::Continue;
    });

    EXPECT_EQ(iterations, std::uint32_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, MultipleForEachIdentifierCacheTest)
{
    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t withdrawn = 0;
    constexpr std::uint32_t GenerateIterations = 1000;

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    for (std::uint32_t idx = 0; idx < GenerateIterations; ++idx) {
        auto const spPeerProxy = m_spManager->LinkPeer(
            Node::Identifier{ Node::GenerateIdentifier() }, Peer::Test::RemoteClientAddress);
        spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});
        if (distribution(generator)) {
            spPeerProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
            ++withdrawn;
        }
    }

    std::set<Node::SharedIdentifier> identifiers;
    {
        std::uint32_t connected = 0;
        m_spManager->ForEach([&] ([[maybe_unused]] Node::SharedIdentifier const& spNodeIdentifier) -> CallbackIteration {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_TRUE(emplaced);
            ++connected;
            return CallbackIteration::Continue;
        }, IPeerCache::Filter::Active);

        EXPECT_EQ(connected, GenerateIterations - withdrawn);
    }

    {
        std::uint32_t disconnected = 0;
        m_spManager->ForEach([&] ([[maybe_unused]] Node::SharedIdentifier const& spNodeIdentifier) -> CallbackIteration {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_TRUE(emplaced);
            ++disconnected;
            return CallbackIteration::Continue;
        }, IPeerCache::Filter::Inactive);
        EXPECT_EQ(disconnected, withdrawn);
    }

    {
        std::uint32_t observed = 0;
        m_spManager->ForEach([&] ([[maybe_unused]] Node::SharedIdentifier const& spNodeIdentifier) -> CallbackIteration {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_FALSE(emplaced);
            ++observed;
            return CallbackIteration::Continue;
        }, IPeerCache::Filter::None);
        EXPECT_EQ(observed, GenerateIterations);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, PeerCountTest)
{
    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    constexpr std::uint32_t GenerateIterations = 1000;

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    for (std::uint32_t idx = 0; idx < GenerateIterations; ++idx) {
        auto const spPeerProxy = m_spManager->LinkPeer(Node::Identifier{ Node::GenerateIdentifier() }, Peer::Test::RemoteClientAddress);
        spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});
        if (distribution(generator)) {
            spPeerProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
            ++disconnected;
        }
    }
    
    EXPECT_EQ(m_spManager->ActiveCount(), GenerateIterations - disconnected);
    EXPECT_EQ(m_spManager->InactiveCount(), disconnected);
    EXPECT_EQ(m_spManager->ObservedCount(), GenerateIterations);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, SingleObserverTest)
{
    Peer::Test::SynchronousObserver synchronous{ m_spManager.get() };
    Peer::Test::AsynchronousObserver asynchronous{ m_spEventPublisher, test::ClientIdentifier };
    ASSERT_TRUE(asynchronous.SubscribedToAllAdvertisedEvents());
    m_spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    auto const spPeerProxy = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);

    // The observers should not be notified of a connected peer when the peer has not yet completed the exchange. 
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Unknown);
    spPeerProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Unknown);

    spPeerProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized); // Simulate an authorized peer. 

    // The observer should be notified of a new endpoint connection when the peer is authorized. 
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Connected);
    spPeerProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::SessionClosure);
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Disconnected);

    EXPECT_TRUE(asynchronous.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, MultipleObserverTest)
{
    Peer::Test::AsynchronousObserver asynchronous{ m_spEventPublisher, test::ClientIdentifier };
    ASSERT_TRUE(asynchronous.SubscribedToAllAdvertisedEvents());
    m_spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    std::vector<Peer::Test::SynchronousObserver> observers;
    for (std::uint32_t idx = 0; idx < 12; ++idx) {
        observers.emplace_back(Peer::Test::SynchronousObserver(m_spManager.get()));
    }

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    auto const spPeerProxy = m_spManager->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    spPeerProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized); // Simulate an authorized peer. 
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});

    for (auto const& synchronous: observers) {
        EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Connected);
    }

    spPeerProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::SessionClosure);

    for (auto const& synchronous: observers) {
        EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Disconnected);
    }

    EXPECT_TRUE(asynchronous.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerManagerSuite, ClusterRequestTest)
{
    constexpr std::size_t GenerateIterations = 16;
    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    for (std::uint32_t idx = 0; idx < GenerateIterations; ++idx) {
        auto const spProxy = m_spManager->LinkPeer(
            Node::Identifier{ Node::GenerateIdentifier() }, Peer::Test::RemoteClientAddress);
        spProxy->RegisterEndpoint(
            identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress,
            [this] (auto const&, auto&&) -> bool { return true; });
    }
    
    std::vector<Message::Application::Parcel> responses;
    auto const onResponse = [&responses] (auto const&, Message::Application::Parcel const& parcel) {
        responses.emplace_back(parcel);
    };

    std::vector<std::tuple<Awaitable::TrackerKey, Node::SharedIdentifier, Peer::Action::Error>> errors;
    auto const onError = [&errors] (auto const& key, auto const& spIdentifier, auto error) {
        errors.emplace_back(std::make_tuple(key, spIdentifier, error));
    };

    auto const optTrackerKey = m_spManager->Request(
        Message::Destination::Cluster, Peer::Test::RequestRoute, Peer::Test::RequestPayload,
        onResponse, onError);
    ASSERT_TRUE(optTrackerKey); 
    EXPECT_NE(*optTrackerKey, Awaitable::TrackerKey{}); 

    auto const context = Peer::Test::GenerateMessageContext();
    bool const responded = m_spManager->ForEach([&] (Node::SharedIdentifier const& spNodeIdentifier) -> CallbackIteration {
        auto optResponse = Message::Application::Parcel::GetBuilder()
            .SetContext(context)
            .SetSource(*spNodeIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(Peer::Test::RequestRoute)
            .SetPayload(Peer::Test::ApplicationPayload)
            .BindExtension<Message::Application::Extension::Awaitable>(
                Message::Application::Extension::Awaitable::Binding::Response, *optTrackerKey)
            .ValidatedBuild();
        if (optResponse) {
            bool const processed = m_spTrackingService->Process(std::move(*optResponse));
            return (processed) ? CallbackIteration::Continue : CallbackIteration::Stop;
        }
        return CallbackIteration::Stop;
    }, IPeerCache::Filter::Active);
    EXPECT_TRUE(responded);

    auto const frames = Scheduler::Frame{ Awaitable::TrackingService::CheckInterval.GetValue() };
    EXPECT_EQ(m_spRegistrar->Run<InvokeContext::Test>(frames), m_spManager->ActiveCount());

    EXPECT_EQ(responses.size(), m_spManager->ActiveCount());
    EXPECT_TRUE(errors.empty());

    for (auto const& response : responses) {
        EXPECT_EQ(response.GetRoute(), Peer::Test::RequestRoute);
        
        auto const optExtension = response.GetExtension<Message::Application::Extension::Awaitable>();
        EXPECT_TRUE(optExtension);
        EXPECT_EQ(optExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);
        EXPECT_EQ(optExtension->get().GetTracker(), *optTrackerKey);  
    }
}

//----------------------------------------------------------------------------------------------------------------------
