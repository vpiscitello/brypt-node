//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Manager.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <iostream>
#include <cstdint>
#include <string>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class ConnectProtocolStub;
class PeerObserverStub;
class MessageCollector;

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
    ConnectProtocolStub();

    // IConnectProtocol {
    virtual bool SendRequest(
        Node::SharedIdentifier const& spSourceIdentifier,
        std::shared_ptr<Peer::Proxy> const& spPeerProxy,
        MessageContext const& context) const override;
    // } IConnectProtocol 

    bool CalledOnce() const;

private:
    mutable std::uint32_t m_count;
};

//----------------------------------------------------------------------------------------------------------------------

class local::PeerObserverStub : public IPeerObserver
{
public:
    explicit PeerObserverStub(IPeerMediator* const mediator)
        : m_mediator(mediator)
        , m_spPeer()
        , m_state(ConnectionState::Unknown)
    {
        m_mediator->RegisterObserver(this);
    }

    PeerObserverStub(PeerObserverStub&& other)
        : m_mediator(other.m_mediator)
        , m_spPeer(std::move(other.m_spPeer))
        , m_state(other.m_state)
    {
        m_mediator->RegisterObserver(this);
    }

    ~PeerObserverStub()
    {
        m_mediator->UnpublishObserver(this);
    }

    // IPeerObserver {
    void HandlePeerStateChange(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        [[maybe_unused]] Network::Endpoint::Identifier identifier,
        [[maybe_unused]] Network::Protocol protocol,
        ConnectionState change) override
    {
        m_state = change;
        switch(m_state) {
            case ConnectionState::Connected: {
                m_spPeer = wpPeerProxy.lock();
            } break;
            case ConnectionState::Disconnected: {
                m_spPeer.reset();
            } break;
            // Not currently testing other connection states for the observer
            default: break;
        }
    }
    // } IPeerObserver

    std::shared_ptr<Peer::Proxy> GetPeerProxy() const
    {
        return m_spPeer;
    }

    ConnectionState GetConnectionState() const
    {
        return m_state;
    }

private:
    IPeerMediator* m_mediator;
    std::shared_ptr<Peer::Proxy> m_spPeer;
    ConnectionState m_state;
};

//----------------------------------------------------------------------------------------------------------------------

class local::MessageCollector : public IMessageSink
{
public:
    MessageCollector();

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        MessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
        MessageContext const& context,
        std::span<std::uint8_t const> buffer) override;
    // }IMessageSink

};

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerDeclarationTest)
{
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, firstAddress);
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});
    ASSERT_TRUE(spPeerProxy);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spPeerProxy->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerMultipleEndpointDisconnectTest)
{
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, firstAddress);
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});
    ASSERT_TRUE(spPeerProxy);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    Network::RemoteAddress secondAddress(Network::Protocol::TCP, "915:71", false);
    manager.LinkPeer(*test::ClientIdentifier, secondAddress);

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(loraIdentifier, Network::Protocol::LoRa, secondAddress, {});
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spPeerProxy->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spPeerProxy->WithdrawEndpoint(loraIdentifier, Network::Protocol::LoRa);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PQNISTL3ExchangeSetupTest)
{
    auto const spConnectProtocol = std::make_shared<local::ConnectProtocolStub>();
    auto const spMessageCollector = std::make_shared<local::MessageCollector>();

    Peer::Manager manager(test::ClientIdentifier, Security::Strategy::PQNISTL3, spConnectProtocol, spMessageCollector);
    EXPECT_EQ(manager.ObservedPeerCount(), std::size_t(0));

    // Declare the client and server peers. 
    std::shared_ptr<Peer::Proxy> spClientPeer;
    std::shared_ptr<Peer::Proxy> spServerPeer;

    // Simulate an endpoint delcaring that it is attempting to resolve a peer at a
    // given uri. 
    auto const optRequest = manager.DeclareResolvingPeer(test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t(0));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    // Simulate the server receiving the connection request. 
    Network::RemoteAddress clientAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    spClientPeer = manager.LinkPeer(*test::ClientIdentifier, clientAddress);
    EXPECT_FALSE(spClientPeer->IsAuthorized());
    EXPECT_FALSE(spClientPeer->IsFlagged());
    EXPECT_EQ(manager.ObservedPeerCount(), std::size_t(1));

    // Create a mock endpoint identifier for the simulated endpoint the client has connected on. 
    auto const clientEndpoint = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    // Create a mock message context for messages passed through the client peer. 
    auto const clientContext = MessageContext(clientEndpoint, Network::Protocol::TCP);

    // Create a mock endpoint identifier for the simulated endpoint the server has responded to. 
    auto const serverEndpoint = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    // Create a mock message context for messages passed through the server peer. 
    auto const serverContext = MessageContext(serverEndpoint, Network::Protocol::TCP);

    // Simulate the server's endpoint registering itself to the given client peer. 
    spClientPeer->RegisterEndpoint(
        clientContext.GetEndpointIdentifier(), clientContext.GetEndpointProtocol(),
        clientAddress,
        [&spServerPeer, &serverContext] (
            [[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            EXPECT_TRUE(spServerPeer->ScheduleReceive(
                serverContext.GetEndpointIdentifier(), std::get<std::string>(message)));
            return true;
        });

    // In practice the client would receive a response from the server before linking a peer. 
    // However, we need to create a peer to properly handle the exchange on the stack. 
    spServerPeer = manager.LinkPeer(*test::ServerIdentifier, test::RemoteServerAddress);
    EXPECT_FALSE(spServerPeer->IsAuthorized());
    EXPECT_FALSE(spServerPeer->IsFlagged());
    EXPECT_EQ(manager.ObservedPeerCount(), std::size_t(2));

    // Simulate the clients's endpoint registering itself to the given server peer. 
    spServerPeer->RegisterEndpoint(
        serverContext.GetEndpointIdentifier(), serverContext.GetEndpointProtocol(),
        test::RemoteServerAddress,
        [&spClientPeer, &clientContext] (
            [[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            EXPECT_TRUE(spClientPeer->ScheduleReceive(
                clientContext.GetEndpointIdentifier(), std::get<std::string>(message)));
            return true;
        });

    // Cause the key exchange setup by the peer manager to occur on the stack. 
    EXPECT_TRUE(spClientPeer->ScheduleReceive(clientContext.GetEndpointIdentifier(), *optRequest));

    // Verify the results of the key exchange
    EXPECT_TRUE(spConnectProtocol->CalledOnce());
    EXPECT_TRUE(spClientPeer->IsAuthorized());
    EXPECT_TRUE(spServerPeer->IsAuthorized());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, SingleForEachIdentiferCacheTest)
{
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, firstAddress);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spPeerProxy);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    manager.ForEachCachedIdentifier(
        [&spPeerProxy] (auto const& spCheckIdentifier) -> CallbackIteration
        {
            EXPECT_EQ(spCheckIdentifier, spPeerProxy->GetNodeIdentifier());
            EXPECT_EQ(*spCheckIdentifier, *spPeerProxy->GetNodeIdentifier());
            return CallbackIteration::Continue;
        });

    spPeerProxy->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);

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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);

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
            spPeerProxy->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);

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
            spPeerProxy->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);
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
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    local::PeerObserverStub observer(&manager);

    EXPECT_FALSE(observer.GetPeerProxy());
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Unknown);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, address);
    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, address, {});

    EXPECT_EQ(observer.GetPeerProxy(), spPeerProxy);
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Connected);

    spPeerProxy->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);

    EXPECT_FALSE(observer.GetPeerProxy());
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Disconnected);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, MultipleObserverTest)
{
    Peer::Manager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);

    std::vector<local::PeerObserverStub> observers;
    for (std::uint32_t idx = 0; idx < 12; ++idx) {
        observers.emplace_back(local::PeerObserverStub(&manager));
    }

    for (auto const& observer: observers) {
        EXPECT_FALSE(observer.GetPeerProxy());
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Unknown);
    }

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance().Generate();

    Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spPeerProxy = manager.LinkPeer(*test::ClientIdentifier, address);
    spPeerProxy->RegisterEndpoint(tcpIdentifier, Network::Protocol::TCP, address, {});

    for (auto const& observer: observers) {
        EXPECT_EQ(observer.GetPeerProxy(), spPeerProxy);
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Connected);
    }

    spPeerProxy->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);

    for (auto const& observer: observers) {
        EXPECT_FALSE(observer.GetPeerProxy());
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Disconnected);
    }
}

//----------------------------------------------------------------------------------------------------------------------

local::ConnectProtocolStub::ConnectProtocolStub()
    : m_count(0)
{
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ConnectProtocolStub::SendRequest(
    [[maybe_unused]] Node::SharedIdentifier const& spSourceIdentifier,
    [[maybe_unused]] std::shared_ptr<Peer::Proxy> const& spPeerProxy,
    [[maybe_unused]] MessageContext const& context) const
{
    ++m_count;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ConnectProtocolStub::CalledOnce() const
{
    return (m_count == 1);
}

//----------------------------------------------------------------------------------------------------------------------

local::MessageCollector::MessageCollector()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool local::MessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    [[maybe_unused]] MessageContext const& context,
    [[maybe_unused]] std::string_view buffer)
{
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::MessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    [[maybe_unused]] MessageContext const& context,
    [[maybe_unused]] std::span<std::uint8_t const> buffer)
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
