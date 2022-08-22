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
#include "Components/Peer/ProxyStore.hpp"
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

class ProxyStoreSuite : public testing::Test
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

        m_spProxyStore = std::make_shared<Peer::ProxyStore>(
            Security::Strategy::PQNISTL3, m_spRegistrar, m_spServiceProvider);
        m_spServiceProvider->Register<IResolutionService>(m_spProxyStore);
     
        EXPECT_TRUE(m_spRegistrar->Initialize());
    }

    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<Peer::Test::ConnectProtocol> m_spConnectProtocol;
    std::shared_ptr<Peer::Test::MessageProcessor> m_spMessageProcessor;
    std::shared_ptr<Peer::ProxyStore> m_spProxyStore;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, PeerDeclarationTest)
{
    EXPECT_EQ(m_spProxyStore->ResolvingCount(), std::size_t{ 0 });
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const optRequest = m_spProxyStore->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });
    EXPECT_EQ(m_spProxyStore->ResolvingCount(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, DuplicatePeerDeclarationTest)
{
    EXPECT_EQ(m_spProxyStore->ResolvingCount(), std::size_t{ 0 });
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const optRequest = m_spProxyStore->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });
    EXPECT_EQ(m_spProxyStore->ResolvingCount(), std::size_t{ 1 });

    auto const optCheckRequest = m_spProxyStore->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    EXPECT_FALSE(optCheckRequest);
    EXPECT_EQ(m_spProxyStore->ResolvingCount(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, UndeclaredPeerTest)
{
    EXPECT_EQ(m_spProxyStore->ResolvingCount(), std::size_t{ 0 });
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const optRequest = m_spProxyStore->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });
    EXPECT_EQ(m_spProxyStore->ResolvingCount(), std::size_t{ 1 });

    m_spProxyStore->RescindResolvingPeer(Peer::Test::RemoteServerAddress);
    EXPECT_EQ(m_spProxyStore->ResolvingCount(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, DeclaredPeerLinkTest)
{
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const optRequest = m_spProxyStore->DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });

    auto const spProxy = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(spProxy);

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_TRUE(spProxy->IsEndpointRegistered(identifier));
    EXPECT_EQ(spProxy->RegisteredEndpointCount(), std::size_t{ 1 });

    // The peer shouldn't be marked as active until it has been authenticated. 
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });
    EXPECT_EQ(m_spProxyStore->ObservedCount(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, UndeclaredPeerLinkTest)
{
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const spProxy = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(spProxy);

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_TRUE(spProxy->IsEndpointRegistered(identifier));
    EXPECT_EQ(spProxy->RegisteredEndpointCount(), std::size_t{ 1 });

    // The peer shouldn't be marked as active until it has been authenticated. 
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });
    EXPECT_EQ(m_spProxyStore->ObservedCount(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, ExistingPeerLinkTest)
{
    m_spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const spCreatedPeer = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spCreatedPeer);
    spCreatedPeer->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);

    {
        auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
        spCreatedPeer->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
        EXPECT_TRUE(spCreatedPeer->IsEndpointRegistered(identifier));
        EXPECT_EQ(spCreatedPeer->RegisteredEndpointCount(), std::size_t{ 1 });
        EXPECT_EQ(m_spProxyStore->ObservedCount(), std::size_t{ 1 });
    }

    {
        auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
        auto const address = Network::RemoteAddress{ Network::Protocol::LoRa, "915:71", false };
        auto const spMergedPeer = m_spProxyStore->LinkPeer(test::ClientIdentifier, address);
        ASSERT_EQ(spMergedPeer, spCreatedPeer);

        spMergedPeer->RegisterEndpoint(identifier, Network::Protocol::LoRa, address, {});
        EXPECT_TRUE(spMergedPeer->IsEndpointRegistered(identifier));
        EXPECT_EQ(spMergedPeer->RegisteredEndpointCount(), std::size_t{ 2 });
        
        // The peer shouldn't be marked as active until it has been authenticated. 
        EXPECT_EQ(m_spProxyStore->ObservedCount(), std::size_t{ 1 });
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, DuplicateEqualSharedPeerLinkTest)
{
    m_spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const spCreatedPeer = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spCreatedPeer);
    spCreatedPeer->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);

    {
        auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
        spCreatedPeer->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
        EXPECT_TRUE(spCreatedPeer->IsEndpointRegistered(identifier));
        EXPECT_EQ(spCreatedPeer->RegisteredEndpointCount(), std::size_t{ 1 });
        EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 1 });
    }

    auto const duplicated = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    {
        auto const address = Network::RemoteAddress{ Network::Protocol::LoRa, "915:71", false };
        auto const spMergedPeer = m_spProxyStore->LinkPeer(test::ClientIdentifier, address);
        ASSERT_EQ(spMergedPeer, spCreatedPeer);

        spMergedPeer->RegisterEndpoint(duplicated, Network::Protocol::LoRa, address, {});
        EXPECT_TRUE(spMergedPeer->IsEndpointRegistered(duplicated));
        EXPECT_EQ(spMergedPeer->RegisteredEndpointCount(), std::size_t{ 2 });
        EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 1 });
    }

    {
        auto const address = Network::RemoteAddress{ Network::Protocol::LoRa, "915:72", false };
        auto const spMergedPeer = m_spProxyStore->LinkPeer(test::ClientIdentifier, address);
        ASSERT_EQ(spMergedPeer, spCreatedPeer);

        spMergedPeer->RegisterEndpoint(duplicated, Network::Protocol::LoRa, address, {});
        EXPECT_TRUE(spMergedPeer->IsEndpointRegistered(duplicated));
        EXPECT_EQ(spMergedPeer->RegisteredEndpointCount(), std::size_t{ 2 });
        EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 1 });
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, PeerSingleEndpointDisconnectTest)
{
    m_spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const spProxy = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spProxy);
    spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 1 });

    spProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, PeerMultipleEndpointDisconnectTest)
{
    m_spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });

    auto const spProxy = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spProxy);
    spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 1 });

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    auto const address = Network::RemoteAddress{ Network::Protocol::LoRa, "915:71", false };
    EXPECT_TRUE(m_spProxyStore->LinkPeer(test::ClientIdentifier, address));
    spProxy->RegisterEndpoint(loraIdentifier, Network::Protocol::LoRa, address, {});
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 1 });

    spProxy->WithdrawEndpoint(tcpIdentifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 1 });

    spProxy->WithdrawEndpoint(loraIdentifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, SingleForEachIdentifierCacheTest)
{
    m_spEventPublisher->SuspendSubscriptions();

    auto const spProxy = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteServerAddress, {});
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 1 });

    m_spProxyStore->ForEach([&] (Node::SharedIdentifier const& spCheckIdentifier) -> CallbackIteration {
        EXPECT_EQ(spCheckIdentifier, spProxy->GetIdentifier());
        EXPECT_EQ(*spCheckIdentifier, *spProxy->GetIdentifier());
        return CallbackIteration::Continue;
    });

    spProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);

    std::uint32_t iterations = 0;
    EXPECT_EQ(m_spProxyStore->ActiveCount(), std::size_t{ 0 });
    m_spProxyStore->ForEach([&] ([[maybe_unused]] Node::SharedIdentifier const& spCheckIdentifier) -> CallbackIteration {
        ++iterations;
        return CallbackIteration::Continue;
    });

    EXPECT_EQ(iterations, std::uint32_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, MultipleForEachIdentifierCacheTest)
{
    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t withdrawn = 0;
    constexpr std::uint32_t GenerateIterations = 10;

    m_spEventPublisher->SuspendSubscriptions();

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    for (std::uint32_t idx = 0; idx < GenerateIterations; ++idx) {
        auto const spProxy = m_spProxyStore->LinkPeer(
            Node::Identifier{ Node::GenerateIdentifier() }, Peer::Test::RemoteClientAddress);
        spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);
        spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});
        if (distribution(generator)) {
            spProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
            ++withdrawn;
        }
    }

    std::set<Node::SharedIdentifier> identifiers;
    {
        std::uint32_t connected = 0;
        m_spProxyStore->ForEach([&] ([[maybe_unused]] Node::SharedIdentifier const& spNodeIdentifier) -> CallbackIteration {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_TRUE(emplaced);
            ++connected;
            return CallbackIteration::Continue;
        }, IPeerCache::Filter::Active);

        EXPECT_EQ(connected, GenerateIterations - withdrawn);
    }

    {
        std::uint32_t disconnected = 0;
        m_spProxyStore->ForEach([&] ([[maybe_unused]] Node::SharedIdentifier const& spNodeIdentifier) -> CallbackIteration {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_TRUE(emplaced);
            ++disconnected;
            return CallbackIteration::Continue;
        }, IPeerCache::Filter::Inactive);
        EXPECT_EQ(disconnected, withdrawn);
    }

    {
        std::uint32_t observed = 0;
        m_spProxyStore->ForEach([&] ([[maybe_unused]] Node::SharedIdentifier const& spNodeIdentifier) -> CallbackIteration {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_FALSE(emplaced);
            ++observed;
            return CallbackIteration::Continue;
        }, IPeerCache::Filter::None);
        EXPECT_EQ(observed, GenerateIterations);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, PeerCountTest)
{
    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    constexpr std::uint32_t GenerateIterations = 10;

    m_spEventPublisher->SuspendSubscriptions();
    
    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    for (std::uint32_t idx = 0; idx < GenerateIterations; ++idx) {
        auto const spProxy = m_spProxyStore->LinkPeer(Node::Identifier{ Node::GenerateIdentifier() }, Peer::Test::RemoteClientAddress);
        spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);
        spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});
        if (distribution(generator)) {
            spProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
            ++disconnected;
        }
    }
    
    EXPECT_EQ(m_spProxyStore->ActiveCount(), GenerateIterations - disconnected);
    EXPECT_EQ(m_spProxyStore->InactiveCount(), disconnected);
    EXPECT_EQ(m_spProxyStore->ObservedCount(), GenerateIterations);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, SingleObserverTest)
{
    Peer::Test::SynchronousObserver synchronous{ m_spProxyStore.get() };
    Peer::Test::AsynchronousObserver asynchronous{ m_spEventPublisher, test::ClientIdentifier };
    ASSERT_TRUE(asynchronous.SubscribedToAllAdvertisedEvents());
    m_spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    auto const spProxy = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);

    // The observers should not be notified of a connected peer when the peer has not yet completed the exchange. 
    spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Unknown);
    spProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::DisconnectRequest);
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Unknown);

    spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized); // Simulate an authorized peer. 

    // The observer should be notified of a new endpoint connection when the peer is authorized. 
    spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Connected);
    spProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::SessionClosure);
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Disconnected);

    EXPECT_TRUE(asynchronous.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, MultipleObserverTest)
{
    Peer::Test::AsynchronousObserver asynchronous{ m_spEventPublisher, test::ClientIdentifier };
    ASSERT_TRUE(asynchronous.SubscribedToAllAdvertisedEvents());
    m_spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    std::vector<Peer::Test::SynchronousObserver> observers;
    for (std::uint32_t idx = 0; idx < 12; ++idx) {
        observers.emplace_back(Peer::Test::SynchronousObserver(m_spProxyStore.get()));
    }

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    auto const spProxy = m_spProxyStore->LinkPeer(test::ClientIdentifier, Peer::Test::RemoteClientAddress);
    spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized); // Simulate an authorized peer. 
    spProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress, {});

    for (auto const& synchronous: observers) {
        EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Connected);
    }

    spProxy->WithdrawEndpoint(identifier, Peer::Proxy::WithdrawalCause::SessionClosure);

    for (auto const& synchronous: observers) {
        EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Disconnected);
    }

    EXPECT_TRUE(asynchronous.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ProxyStoreSuite, ClusterRequestTest)
{
    m_spEventPublisher->SuspendSubscriptions();

    constexpr std::size_t GenerateIterations = 16;
    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    for (std::uint32_t idx = 0; idx < GenerateIterations; ++idx) {
        auto const spProxy = m_spProxyStore->LinkPeer(
            Node::Identifier{ Node::GenerateIdentifier() }, Peer::Test::RemoteClientAddress);
        spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);
        spProxy->RegisterEndpoint(
            identifier, Network::Protocol::TCP, Peer::Test::RemoteClientAddress,
            [this] (auto const&, auto&&) -> bool { return true; });
    }
    
    // The event publisher will have some work to do.
    EXPECT_EQ(m_spRegistrar->Execute(), m_spProxyStore->ActiveCount());

    auto const onResponse = [&] (Peer::Action::Response const& response) {
        EXPECT_TRUE(m_spProxyStore->Find(static_cast<std::string const&>(response.GetSource())));
        EXPECT_EQ(response.GetPayload(), Peer::Test::ApplicationPayload);
        EXPECT_EQ(response.GetStatusCode(), Message::Extension::Status::Created);
    };

    auto const onError = [&] (Peer::Action::Response const&) {
        EXPECT_FALSE(true);
    };

    auto const optResult = m_spProxyStore->Request(
        Message::Destination::Cluster, Peer::Test::RequestRoute, Peer::Test::RequestPayload,
        onResponse, onError);
    ASSERT_TRUE(optResult); 
    EXPECT_NE(optResult->first, Awaitable::TrackerKey{}); 
    EXPECT_EQ(optResult->second, GenerateIterations); 

    bool const responded = m_spProxyStore->ForEach([&] (std::shared_ptr<Peer::Proxy> const& spProxy) -> CallbackIteration {
        auto const optContext = spProxy->GetMessageContext(identifier);
        if (!optContext) { return CallbackIteration::Stop; }

        auto optResponse = Message::Application::Parcel::GetBuilder()
            .SetContext(*optContext)
            .SetSource(spProxy->GetIdentifier<Node::External::Identifier>())
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(Peer::Test::RequestRoute)
            .SetPayload(Peer::Test::ApplicationPayload)
            .BindExtension<Message::Extension::Awaitable>(
                Message::Extension::Awaitable::Binding::Response, optResult->first)
            .BindExtension<Message::Extension::Status>(
                Message::Extension::Status::Created)
            .ValidatedBuild();

        if (optResponse) {
            bool const processed = m_spTrackingService->Process(std::move(*optResponse));
            return (processed) ? CallbackIteration::Continue : CallbackIteration::Stop;
        }

        return CallbackIteration::Stop;
    }, IPeerCache::Filter::Active);
    EXPECT_TRUE(responded);

    auto const frames = Scheduler::Frame{ Awaitable::TrackingService::CheckInterval.GetValue() };
    EXPECT_EQ(m_spRegistrar->Run<InvokeContext::Test>(frames), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------
