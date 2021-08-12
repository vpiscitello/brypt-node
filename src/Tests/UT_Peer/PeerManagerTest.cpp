//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Manager.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Resolver.hpp"
#include "Components/Scheduler/Service.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <iostream>
#include <cstdint>
#include <random>
#include <ranges>
#include <string>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class ConnectProtocolStub;
class MessageCollector;
class SynchronousObserver;
class AsynchronousObserver;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());

Network::RemoteAddress const RemoteServerAddress(Network::Protocol::TCP, "127.0.0.1:35216", true);
constexpr std::string_view const ConnectMessage = "Connection Request";

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::ConnectProtocolStub : public IConnectProtocol
{
public:
    ConnectProtocolStub() : m_count(0) {}

    // IConnectProtocol {
    virtual bool SendRequest(
        Node::SharedIdentifier const&, std::shared_ptr<Peer::Proxy> const&, MessageContext const&) const override 
    {
        ++m_count;
        return true;
    }
    // } IConnectProtocol 

    bool CalledOnce() const { return m_count == 1; }

private:
    mutable std::uint32_t m_count;
};

//----------------------------------------------------------------------------------------------------------------------

class local::MessageCollector : public IMessageSink
{
public:
    MessageCollector() = default;

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const&, MessageContext const&, std::string_view) override { return true; }
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const&, MessageContext const&, std::span<std::uint8_t const>) override { return false; }
    // }IMessageSink
};

//----------------------------------------------------------------------------------------------------------------------

class local::SynchronousObserver : public IPeerObserver
{
public:
    explicit SynchronousObserver(IPeerMediator* const mediator);
    SynchronousObserver(SynchronousObserver&& other);
    ~SynchronousObserver();

    // IPeerObserver {
    void OnRemoteConnected(Network::Endpoint::Identifier, Network::RemoteAddress const&) override;
    void OnRemoteDisconnected(Network::Endpoint::Identifier, Network::RemoteAddress const&) override;
    // } IPeerObserver

    Network::Connection::State GetConnectionState() const;

private:
    IPeerMediator* m_mediator;
    Network::Connection::State m_state;
};

//----------------------------------------------------------------------------------------------------------------------

class local::AsynchronousObserver
{
public:
    using EventRecord = std::vector<Event::Type>;
    using EventTracker = std::unordered_map<Node::Identifier, EventRecord, Node::IdentifierHasher>;

    AsynchronousObserver(Event::SharedPublisher const& spPublisher, Node::Identifier const& identifier);
    [[nodiscard]] bool SubscribedToAllAdvertisedEvents() const;
    [[nodiscard]] bool ReceivedExpectedEventSequence() const;

private:
    constexpr static std::uint32_t ExpectedEventCount = 2; // The number of events each peer should fire. 
    Event::SharedPublisher m_spPublisher;
    EventTracker m_tracker;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerDeclarationTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(0));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t(0));
    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(1));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DuplicatePeerDeclarationTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(0));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t(0));
    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(1));

    auto const optCheckRequest = manager.DeclareResolvingPeer(test::RemoteServerAddress);
    EXPECT_FALSE(optCheckRequest);
    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(1));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, UndeclarePeerTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(0));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t(0));
    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(1));

    manager.RescindResolvingPeer(test::RemoteServerAddress);
    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DeclaredPeerLinkTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t(0));

    auto const spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, test::RemoteServerAddress);
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spPeerProxy);
    EXPECT_TRUE(spPeerProxy->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spPeerProxy->RegisteredEndpointCount(), std::size_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, UndeclaredPeerLinkTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto const spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, address);
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spPeerProxy);
    EXPECT_TRUE(spPeerProxy->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spPeerProxy->RegisteredEndpointCount(), std::size_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, ExistingPeerLinkTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto const spFirstPeer = manager.LinkPeer(*test::ClientIdentifier, firstAddress);
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spFirstPeer->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    Network::RemoteAddress secondAddress(Network::Protocol::TCP, "915:71", false);
    auto const spSecondPeer = manager.LinkPeer(*test::ClientIdentifier, secondAddress);
    spSecondPeer->RegisterEndpoint(loraIdentifier, Network::Protocol::LoRa, secondAddress, {});

    EXPECT_EQ(spSecondPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DuplicateEqualSharedPeerLinkTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto const spFirstPeer = manager.LinkPeer(*test::ClientIdentifier, firstAddress);
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spFirstPeer->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    Network::RemoteAddress secondAddress(Network::Protocol::TCP, "915:71", false);
    auto const spSecondPeer = manager.LinkPeer(*test::ClientIdentifier, secondAddress);
    spSecondPeer->RegisterEndpoint(loraIdentifier, Network::Protocol::LoRa, secondAddress, {});

    EXPECT_EQ(spSecondPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    Network::RemoteAddress thirdAddress(Network::Protocol::TCP, "915:72", false);
    auto const spThirdPeer = manager.LinkPeer(*test::ClientIdentifier, thirdAddress);
    spThirdPeer->RegisterEndpoint(loraIdentifier, Network::Protocol::LoRa, thirdAddress, {});

    EXPECT_EQ(spThirdPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerSingleEndpointDisconnectTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, firstAddress);
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});
    ASSERT_TRUE(spPeerProxy);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spPeerProxy->WithdrawEndpoint(tcpIdentifier);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerMultipleEndpointDisconnectTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, firstAddress);
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});
    ASSERT_TRUE(spPeerProxy);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    Network::RemoteAddress secondAddress(Network::Protocol::TCP, "915:71", false);
    EXPECT_TRUE(manager.LinkPeer(*test::ClientIdentifier, secondAddress));

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(loraIdentifier, Network::Protocol::LoRa, secondAddress, {});
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spPeerProxy->WithdrawEndpoint(tcpIdentifier);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spPeerProxy->WithdrawEndpoint(loraIdentifier);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PQNISTL3ExchangeSetupTest)
{
    using enum Security::Strategy;
    
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spPublisher = std::make_shared<Event::Publisher>(spScheduler);
    auto const spProcessor = std::make_shared<local::MessageCollector>();
    auto const spProtocol = std::make_shared<local::ConnectProtocolStub>();
    spPublisher->SuspendSubscriptions();

    Peer::Manager client(test::ClientIdentifier, PQNISTL3, spPublisher, spProtocol, spProcessor);
    MessageContext const serverContext = { 
        Network::Endpoint::IdentifierGenerator::Instance().Generate(), Network::Protocol::TCP };
    std::shared_ptr<Peer::Proxy> spServerProxy; // The server peer is associated with the client's manager. 

    Peer::Manager server(test::ServerIdentifier, PQNISTL3, spPublisher, spProtocol, spProcessor);
    MessageContext const clientContext = { 
        Network::Endpoint::IdentifierGenerator::Instance().Generate(), Network::Protocol::TCP };
    std::shared_ptr<Peer::Proxy> spClientProxy; // The client peer is associated with the server's manager. 

    // Simulate an endpoint delcaring that it is attempting to resolve a peer at a given uri. 
    auto const optRequest = client.DeclareResolvingPeer(test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t(0));
    EXPECT_EQ(client.ActivePeerCount(), std::size_t(0));

    // Simulate the server receiving the connection request. 
    Network::RemoteAddress clientAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    spClientProxy = server.LinkPeer(*test::ClientIdentifier, clientAddress);
    EXPECT_FALSE(spClientProxy->IsAuthorized());
    EXPECT_FALSE(spClientProxy->IsFlagged());
    EXPECT_EQ(server.ObservedPeerCount(), std::size_t(1));

    // Simulate the server's endpoint registering itself to the given client peer. 
    spClientProxy->RegisterEndpoint(
        clientContext.GetEndpointIdentifier(), clientContext.GetEndpointProtocol(), clientAddress,
        [&spServerProxy, &serverContext] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            EXPECT_TRUE(spServerProxy->ScheduleReceive(
                serverContext.GetEndpointIdentifier(), std::get<std::string>(message)));
            return true;
        });

    // In practice the client would receive a response from the server before linking a peer. 
    // However, we need to create a peer to properly handle the exchange on the stack. 
    spServerProxy = client.LinkPeer(*test::ServerIdentifier, test::RemoteServerAddress);
    EXPECT_FALSE(spServerProxy->IsAuthorized());
    EXPECT_FALSE(spServerProxy->IsFlagged());
    EXPECT_EQ(client.ObservedPeerCount(), std::size_t(1));

    // Simulate the clients's endpoint registering itself to the given server peer. 
    spServerProxy->RegisterEndpoint(
        serverContext.GetEndpointIdentifier(), serverContext.GetEndpointProtocol(), test::RemoteServerAddress,
        [&spClientProxy, &clientContext] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            EXPECT_TRUE(spClientProxy->ScheduleReceive(
                clientContext.GetEndpointIdentifier(), std::get<std::string>(message)));
            return true;
        });

    // Cause the key exchange setup by the peer manager to occur on the stack. 
    EXPECT_TRUE(spClientProxy->ScheduleReceive(clientContext.GetEndpointIdentifier(), *optRequest));

    // Verify the results of the key exchange
    EXPECT_TRUE(spProtocol->CalledOnce());
    EXPECT_TRUE(spClientProxy->IsAuthorized());
    EXPECT_TRUE(spServerProxy->IsAuthorized());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, SingleForEachIdentiferCacheTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, firstAddress);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spPeerProxy);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    manager.ForEachCachedIdentifier(
        [&spPeerProxy] (auto const& spCheckIdentifier) -> CallbackIteration
        {
            EXPECT_EQ(spCheckIdentifier, spPeerProxy->GetIdentifier());
            EXPECT_EQ(*spCheckIdentifier, *spPeerProxy->GetIdentifier());
            return CallbackIteration::Continue;
        });

    spPeerProxy->WithdrawEndpoint(tcpIdentifier);

    std::uint32_t iterations = 0;
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));
    manager.ForEachCachedIdentifier(
        [&iterations] ([[maybe_unused]] auto const& spCheckIdentifier) -> CallbackIteration
        {
            ++iterations;
            return CallbackIteration::Continue;
        });
    EXPECT_EQ(iterations, std::uint32_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, MultipleForEachIdentiferCacheTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    std::uint32_t const iterations = 1000;

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    for (std::uint32_t idx = 0; idx < iterations; ++idx) {
        Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
        auto spPeerProxy = manager.LinkPeer(Node::Identifier{ Node::GenerateIdentifier() }, address);
        spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, address, {});
        if (distribution(generator)) {
            spPeerProxy->WithdrawEndpoint(tcpIdentifier);
            ++disconnected;
        }
    }

    std::set<Node::SharedIdentifier> identifiers;
    std::uint32_t connectedIterations = 0;
    manager.ForEachCachedIdentifier(
        [&identifiers, &connectedIterations] (
            [[maybe_unused]] auto const& spNodeIdentifier) -> CallbackIteration
        {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_TRUE(emplaced);
            ++connectedIterations;
            return CallbackIteration::Continue;
        },
        IPeerCache::Filter::Active);
    EXPECT_EQ(connectedIterations, iterations - disconnected);

    std::uint32_t disconnectedIterations = 0;
    manager.ForEachCachedIdentifier(
        [&identifiers, &disconnectedIterations] (
            [[maybe_unused]] auto const& spNodeIdentifier) -> CallbackIteration
        {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_TRUE(emplaced);
            ++disconnectedIterations;
            return CallbackIteration::Continue;
        },
        IPeerCache::Filter::Inactive);
    EXPECT_EQ(disconnectedIterations, disconnected);

    std::uint32_t observedIterations = 0;
    manager.ForEachCachedIdentifier(
        [&identifiers, &observedIterations] (
            [[maybe_unused]] auto const& spNodeIdentifier) -> CallbackIteration
        {
            auto const [itr, emplaced] = identifiers.emplace(spNodeIdentifier);
            EXPECT_FALSE(emplaced);
            ++observedIterations;
            return CallbackIteration::Continue;
        },
        IPeerCache::Filter::None);
    EXPECT_EQ(observedIterations, iterations);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerCountTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);
    spEventPublisher->SuspendSubscriptions();

    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    std::uint32_t const iterations = 1000;
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    for (std::uint32_t idx = 0; idx < iterations; ++idx) {
        Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
        auto spPeerProxy = manager.LinkPeer(Node::Identifier{ Node::GenerateIdentifier() }, address);
        spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, address, {});
        if (distribution(generator)) {
            spPeerProxy->WithdrawEndpoint(tcpIdentifier);
            ++disconnected;
        }
    }
    EXPECT_EQ(manager.ActivePeerCount(), iterations - disconnected);
    EXPECT_EQ(manager.InactivePeerCount(), disconnected);
    EXPECT_EQ(manager.ObservedPeerCount(), iterations);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, SingleObserverTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);

    local::SynchronousObserver synchronous(&manager);
    local::AsynchronousObserver asynchronous(spEventPublisher, *test::ClientIdentifier);
    ASSERT_TRUE(asynchronous.SubscribedToAllAdvertisedEvents());
    spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, address);

    // The observers should not be notified of a connected peer when the peer has not yet completed the exchange. 
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, address, {});
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Unknown);
    spPeerProxy->WithdrawEndpoint(identifier);
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Unknown);

    spPeerProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized); // Simulate an authorized peer. 

    // The observer should be notified of a new endpoint connection when the peer is authorized. 
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, address, {});
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Connected);
    spPeerProxy->WithdrawEndpoint(identifier);
    EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Disconnected);

    EXPECT_TRUE(asynchronous.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, MultipleObserverTest)
{
    auto const spScheduler = std::make_shared<Scheduler::Service>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spScheduler);
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, spEventPublisher, nullptr);

    local::AsynchronousObserver asynchronous(spEventPublisher, *test::ClientIdentifier);
    ASSERT_TRUE(asynchronous.SubscribedToAllAdvertisedEvents());
    spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    std::vector<local::SynchronousObserver> observers;
    for (std::uint32_t idx = 0; idx < 12; ++idx) {
        observers.emplace_back(local::SynchronousObserver(&manager));
    }

    auto const identifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();
    Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, address);
    spPeerProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized); // Simulate an authorized peer. 
    spPeerProxy->RegisterEndpoint(identifier, Network::Protocol::TCP, address, {});

    for (auto const& synchronous: observers) {
        EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Connected);
    }

    spPeerProxy->WithdrawEndpoint(identifier);

    for (auto const& synchronous: observers) {
        EXPECT_EQ(synchronous.GetConnectionState(), Network::Connection::State::Disconnected);
    }

    EXPECT_TRUE(asynchronous.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

local::SynchronousObserver::SynchronousObserver(IPeerMediator* const mediator)
    : m_mediator(mediator)
    , m_state(Network::Connection::State::Unknown)
{
    m_mediator->RegisterObserver(this);
}

//----------------------------------------------------------------------------------------------------------------------

local::SynchronousObserver::SynchronousObserver(SynchronousObserver&& other)
    : m_mediator(other.m_mediator)
    , m_state(other.m_state)
{
    m_mediator->RegisterObserver(this);
}

//----------------------------------------------------------------------------------------------------------------------

local::SynchronousObserver::~SynchronousObserver() { m_mediator->UnpublishObserver(this); }

//----------------------------------------------------------------------------------------------------------------------

void local::SynchronousObserver::OnRemoteConnected(Network::Endpoint::Identifier, Network::RemoteAddress const&)
{
    m_state = Network::Connection::State::Connected;
}

//----------------------------------------------------------------------------------------------------------------------

void local::SynchronousObserver::OnRemoteDisconnected(Network::Endpoint::Identifier, Network::RemoteAddress const&)
{
    m_state = Network::Connection::State::Disconnected;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Connection::State local::SynchronousObserver::GetConnectionState() const { return m_state; }

//----------------------------------------------------------------------------------------------------------------------

local::AsynchronousObserver::AsynchronousObserver(
    Event::SharedPublisher const& spPublisher, Node::Identifier const& identifier)
    : m_spPublisher(spPublisher)
    , m_tracker()
{
    using DisconnectCause = Event::Message<Event::Type::PeerDisconnected>::Cause;

    m_tracker.emplace(identifier, EventRecord{}); // Make an event record using the provided peer identifier. 

    // Subscribe to all events fired by an endpoint. Each listener should only record valid events. 
    spPublisher->Subscribe<Event::Type::PeerConnected>(
        [&tracker = m_tracker]
        (Network::Protocol protocol, Node::SharedIdentifier const& spIdentifier)
        {
            if (protocol == Network::Protocol::Invalid || !spIdentifier) { return; }
            if (auto const itr = tracker.find(*spIdentifier); itr != tracker.end()) {
                itr->second.emplace_back(Event::Type::PeerConnected);
            }
        });

    spPublisher->Subscribe<Event::Type::PeerDisconnected>(
        [&tracker = m_tracker] 
        (Network::Protocol protocol, Node::SharedIdentifier const& spIdentifier, DisconnectCause cause)
        {
            if (protocol == Network::Protocol::Invalid || !spIdentifier) { return; }
            if (cause != DisconnectCause::SessionClosure) { return; }
            if (auto const itr = tracker.find(*spIdentifier); itr != tracker.end()) {
                itr->second.emplace_back(Event::Type::PeerDisconnected);
            }
        });
}

//----------------------------------------------------------------------------------------------------------------------

bool local::AsynchronousObserver::SubscribedToAllAdvertisedEvents() const
{
    // We expect to be subscribed to all events advertised by an endpoint. A failure here is most likely caused
    // by this test fixture being outdated. 
    return m_spPublisher->ListenerCount() == m_spPublisher->AdvertisedCount();
}

//----------------------------------------------------------------------------------------------------------------------

bool local::AsynchronousObserver::ReceivedExpectedEventSequence() const
{
    if (m_spPublisher->Dispatch() == 0) { return false; } // We expect that events have been published. 

    // Count the number of peers that match the expected number and sequence of events (e.g. start becomes stop). 
    std::size_t const count = std::ranges::count_if(m_tracker | std::views::values,
        [] (EventRecord const& record) -> bool
        {
            if (record.size() != ExpectedEventCount) { return false; }
            if (record[0] != Event::Type::PeerConnected) { return false; }
            if (record[1] != Event::Type::PeerDisconnected) { return false; }
            return true;
        });

    // We expect that all endpoints tracked meet the event sequence expectations. 
    if (count != m_tracker.size()) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
