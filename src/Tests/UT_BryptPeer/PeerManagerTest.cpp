//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/BryptPeer/PeerManager.hpp"
#include "Components/Network/ConnectionState.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/PeerMediator.hpp"
#include "Interfaces/PeerObserver.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//------------------------------------------------------------------------------------------------
#include <iostream>
#include <cstdint>
#include <string>
#include <random>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

class ConnectProtocolStub;
class PeerObserverStub;
class MessageCollector;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<BryptIdentifier::Container>(
    BryptIdentifier::Generate());
auto const ServerIdentifier = std::make_shared<BryptIdentifier::Container>(
    BryptIdentifier::Generate());

Network::RemoteAddress const RemoteServerAddress(Network::Protocol::TCP, "127.0.0.1:35216", true);
constexpr std::string_view const ConnectMessage = "Connection Request";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::ConnectProtocolStub : public IConnectProtocol
{
public:
    ConnectProtocolStub();

    // IConnectProtocol {
    virtual bool SendRequest(
        BryptIdentifier::SharedContainer const& spSourceIdentifier,
        std::shared_ptr<BryptPeer> const& spBryptPeer,
        MessageContext const& context) const override;
    // } IConnectProtocol 

    bool CalledOnce() const;

private:
    mutable std::uint32_t m_count;
};

//------------------------------------------------------------------------------------------------

class local::PeerObserverStub : public IPeerObserver
{
public:
    explicit PeerObserverStub(IPeerMediator* const mediator)
        : m_mediator(mediator)
        , m_spBryptPeer()
        , m_state(ConnectionState::Unknown)
    {
        m_mediator->RegisterObserver(this);
    }

    PeerObserverStub(PeerObserverStub&& other)
        : m_mediator(other.m_mediator)
        , m_spBryptPeer(std::move(other.m_spBryptPeer))
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
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        [[maybe_unused]] Network::Endpoint::Identifier identifier,
        [[maybe_unused]] Network::Protocol protocol,
        ConnectionState change) override
    {
        m_state = change;
        switch(m_state) {
            case ConnectionState::Connected: {
                m_spBryptPeer = wpBryptPeer.lock();
            } break;
            case ConnectionState::Disconnected: {
                m_spBryptPeer.reset();
            } break;
            // Not currently testing other connection states for the observer
            default: break;
        }
    }
    // } IPeerObserver

    std::shared_ptr<BryptPeer> GetBryptPeer() const
    {
        return m_spBryptPeer;
    }

    ConnectionState GetConnectionState() const
    {
        return m_state;
    }

private:
    IPeerMediator* m_mediator;
    std::shared_ptr<BryptPeer> m_spBryptPeer;
    ConnectionState m_state;
};

//------------------------------------------------------------------------------------------------

class local::MessageCollector : public IMessageSink
{
public:
    MessageCollector();

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        MessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        MessageContext const& context,
        std::span<std::uint8_t const> buffer) override;
    // }IMessageSink

};

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerDeclarationTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(0));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t(0));
    EXPECT_EQ(manager.ResolvingPeerCount(), std::size_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DuplicatePeerDeclarationTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
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

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DeclaredPeerLinkTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t(0));

    auto const spBryptPeer = manager.LinkPeer(*test::ClientIdentifier, test::RemoteServerAddress);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spBryptPeer);
    EXPECT_TRUE(spBryptPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spBryptPeer->RegisteredEndpointCount(), std::size_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, UndeclaredPeerLinkTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto const spBryptPeer = manager.LinkPeer(*test::ClientIdentifier, address);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spBryptPeer);
    EXPECT_TRUE(spBryptPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spBryptPeer->RegisteredEndpointCount(), std::size_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, ExistingPeerLinkTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto const spFirstPeer = manager.LinkPeer(*test::ClientIdentifier, firstAddress);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    spFirstPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    Network::RemoteAddress secondAddress(Network::Protocol::TCP, "915:71", false);
    auto const spSecondPeer = manager.LinkPeer(*test::ClientIdentifier, secondAddress);
    spSecondPeer->RegisterEndpoint(
        loraIdentifier, Network::Protocol::LoRa, secondAddress, {});

    EXPECT_EQ(spSecondPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DuplicateEqualSharedPeerLinkTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto const spFirstPeer = manager.LinkPeer(*test::ClientIdentifier, firstAddress);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    spFirstPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    Network::RemoteAddress secondAddress(Network::Protocol::TCP, "915:71", false);
    auto const spSecondPeer = manager.LinkPeer(*test::ClientIdentifier, secondAddress);
    spSecondPeer->RegisterEndpoint(
        loraIdentifier, Network::Protocol::LoRa, secondAddress, {});

    EXPECT_EQ(spSecondPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    Network::RemoteAddress thirdAddress(Network::Protocol::TCP, "915:72", false);
    auto const spThirdPeer = manager.LinkPeer(*test::ClientIdentifier, thirdAddress);
    spThirdPeer->RegisterEndpoint(
        loraIdentifier, Network::Protocol::LoRa, thirdAddress, {});

    EXPECT_EQ(spThirdPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::size_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerSingleEndpointDisconnectTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier, firstAddress);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerMultipleEndpointDisconnectTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier, firstAddress);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    Network::RemoteAddress secondAddress(Network::Protocol::TCP, "915:71", false);
    manager.LinkPeer(*test::ClientIdentifier, secondAddress);

    auto const loraIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    spBryptPeer->RegisterEndpoint(
        loraIdentifier, Network::Protocol::LoRa, secondAddress, {});

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    spBryptPeer->WithdrawEndpoint(loraIdentifier, Network::Protocol::LoRa);

    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PQNISTL3ExchangeSetupTest)
{
    auto const spConnectProtocol = std::make_shared<local::ConnectProtocolStub>();
    auto const spMessageCollector = std::make_shared<local::MessageCollector>();

    PeerManager manager(
        test::ClientIdentifier, Security::Strategy::PQNISTL3, spConnectProtocol, spMessageCollector);
    EXPECT_EQ(manager.ObservedPeerCount(), std::size_t(0));

    // Declare the client and server peers. 
    std::shared_ptr<BryptPeer> spClientPeer;
    std::shared_ptr<BryptPeer> spServerPeer;

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
    auto const clientEndpoint = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    // Create a mock message context for messages passed through the client peer. 
    auto const clientContext = MessageContext(clientEndpoint, Network::Protocol::TCP);

    // Create a mock endpoint identifier for the simulated endpoint the server has responded to. 
    auto const serverEndpoint = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    // Create a mock message context for messages passed through the server peer. 
    auto const serverContext = MessageContext(serverEndpoint, Network::Protocol::TCP);

    // Simulate the server's endpoint registering itself to the given client peer. 
    spClientPeer->RegisterEndpoint(
        clientContext.GetEndpointIdentifier(), clientContext.GetEndpointProtocol(),
        clientAddress,
        [&spServerPeer, &serverContext] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            EXPECT_TRUE(spServerPeer->ScheduleReceive(
                serverContext.GetEndpointIdentifier(), message));
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
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            EXPECT_TRUE(spClientPeer->ScheduleReceive(
                clientContext.GetEndpointIdentifier(), message));
            return true;
        });

    // Cause the key exchange setup by the peer manager to occur on the stack. 
    EXPECT_TRUE(spClientPeer->ScheduleReceive(
        clientContext.GetEndpointIdentifier(), *optRequest));

    // Verify the results of the key exchange
    EXPECT_TRUE(spConnectProtocol->CalledOnce());
    EXPECT_TRUE(spClientPeer->IsAuthorized());
    EXPECT_TRUE(spServerPeer->IsAuthorized());
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, SingleForEachIdentiferCacheTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);

    Network::RemoteAddress firstAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier, firstAddress);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, test::RemoteServerAddress, {});

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::size_t(1));

    manager.ForEachCachedIdentifier(
        [&spBryptPeer] (auto const& spCheckIdentifier) -> CallbackIteration
        {
            EXPECT_EQ(spCheckIdentifier, spBryptPeer->GetBryptIdentifier());
            EXPECT_EQ(*spCheckIdentifier, *spBryptPeer->GetBryptIdentifier());
            return CallbackIteration::Continue;
        });

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);

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

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, MultipleForEachIdentiferCacheTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);

    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    std::uint32_t const iterations = 1000;

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    for (std::uint32_t idx = 0; idx < iterations; ++idx) {
        Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
        auto spBryptPeer = manager.LinkPeer(
            BryptIdentifier::Container{ BryptIdentifier::Generate() }, address);
        spBryptPeer->RegisterEndpoint(
            tcpIdentifier, Network::Protocol::TCP, address, {});
        if (distribution(generator)) {
            spBryptPeer->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);
            ++disconnected;
        }
    }

    std::set<BryptIdentifier::SharedContainer> identifiers;
    std::uint32_t connectedIterations = 0;
    manager.ForEachCachedIdentifier(
        [&identifiers, &connectedIterations] (
            [[maybe_unused]] auto const& spBryptIdentifier) -> CallbackIteration
        {
            auto const [itr, emplaced] = identifiers.emplace(spBryptIdentifier);
            EXPECT_TRUE(emplaced);
            ++connectedIterations;
            return CallbackIteration::Continue;
        },
        IPeerCache::Filter::Active);
    EXPECT_EQ(connectedIterations, iterations - disconnected);

    std::uint32_t disconnectedIterations = 0;
    manager.ForEachCachedIdentifier(
        [&identifiers, &disconnectedIterations] (
            [[maybe_unused]] auto const& spBryptIdentifier) -> CallbackIteration
        {
            auto const [itr, emplaced] = identifiers.emplace(spBryptIdentifier);
            EXPECT_TRUE(emplaced);
            ++disconnectedIterations;
            return CallbackIteration::Continue;
        },
        IPeerCache::Filter::Inactive);
    EXPECT_EQ(disconnectedIterations, disconnected);

    std::uint32_t observedIterations = 0;
    manager.ForEachCachedIdentifier(
        [&identifiers, &observedIterations] (
            [[maybe_unused]] auto const& spBryptIdentifier) -> CallbackIteration
        {
            auto const [itr, emplaced] = identifiers.emplace(spBryptIdentifier);
            EXPECT_FALSE(emplaced);
            ++observedIterations;
            return CallbackIteration::Continue;
        },
        IPeerCache::Filter::None);
    EXPECT_EQ(observedIterations, iterations);
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerCountTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);

    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    std::uint32_t const iterations = 1000;
    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    for (std::uint32_t idx = 0; idx < iterations; ++idx) {
        Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
        auto spBryptPeer = manager.LinkPeer(
            BryptIdentifier::Container{ BryptIdentifier::Generate() }, address);
        spBryptPeer->RegisterEndpoint(
            tcpIdentifier, Network::Protocol::TCP, address, {});
        if (distribution(generator)) {
            spBryptPeer->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);
            ++disconnected;
        }
    }
    EXPECT_EQ(manager.ActivePeerCount(), iterations - disconnected);
    EXPECT_EQ(manager.InactivePeerCount(), disconnected);
    EXPECT_EQ(manager.ObservedPeerCount(), iterations);
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, SingleObserverTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);
    local::PeerObserverStub observer(&manager);

    EXPECT_FALSE(observer.GetBryptPeer());
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Unknown);

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier, address);
    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, address, {});

    EXPECT_EQ(observer.GetBryptPeer(), spBryptPeer);
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Connected);

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);

    EXPECT_FALSE(observer.GetBryptPeer());
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Disconnected);
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, MultipleObserverTest)
{
    PeerManager manager(test::ServerIdentifier, Security::Strategy::PQNISTL3, nullptr);

    std::vector<local::PeerObserverStub> observers;
    for (std::uint32_t idx = 0; idx < 12; ++idx) {
        observers.emplace_back(local::PeerObserverStub(&manager));
    }

    for (auto const& observer: observers) {
        EXPECT_FALSE(observer.GetBryptPeer());
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Unknown);
    }

    auto const tcpIdentifier = Network::Endpoint::IdentifierGenerator::Instance()
        .Generate();

    Network::RemoteAddress address(Network::Protocol::TCP, "127.0.0.1:35217", false);
    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier, address);
    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Network::Protocol::TCP, address, {});

    for (auto const& observer: observers) {
        EXPECT_EQ(observer.GetBryptPeer(), spBryptPeer);
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Connected);
    }

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Network::Protocol::TCP);

    for (auto const& observer: observers) {
        EXPECT_FALSE(observer.GetBryptPeer());
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Disconnected);
    }
}

//------------------------------------------------------------------------------------------------

local::ConnectProtocolStub::ConnectProtocolStub()
    : m_count(0)
{
}

//------------------------------------------------------------------------------------------------

bool local::ConnectProtocolStub::SendRequest(
    [[maybe_unused]] BryptIdentifier::SharedContainer const& spSourceIdentifier,
    [[maybe_unused]] std::shared_ptr<BryptPeer> const& spBryptPeer,
    [[maybe_unused]] MessageContext const& context) const
{
    ++m_count;
    return true;
}

//------------------------------------------------------------------------------------------------

bool local::ConnectProtocolStub::CalledOnce() const
{
    return (m_count == 1);
}

//------------------------------------------------------------------------------------------------

local::MessageCollector::MessageCollector()
{
}

//------------------------------------------------------------------------------------------------

bool local::MessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<BryptPeer> const& wpBryptPeer,
    [[maybe_unused]] MessageContext const& context,
    [[maybe_unused]] std::string_view buffer)
{
    return true;
}

//------------------------------------------------------------------------------------------------

bool local::MessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<BryptPeer> const& wpBryptPeer,
    [[maybe_unused]] MessageContext const& context,
    [[maybe_unused]] std::span<std::uint8_t const> buffer)
{
    return false;
}

//------------------------------------------------------------------------------------------------
