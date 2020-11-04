//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../BryptMessage/MessageContext.hpp"
#include "../../Components/BryptPeer/PeerManager.hpp"
#include "../../Components/Endpoints/ConnectionState.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Interfaces/ConnectProtocol.hpp"
#include "../../Interfaces/PeerMediator.hpp"
#include "../../Interfaces/PeerObserver.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
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

class CConnectProtocolStub;
class CPeerObserverStub;
class CMessageCollector;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());
auto const ServerIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());

constexpr std::string_view ServerEntry = "127.0.0.1:35216";

constexpr std::string_view const ConnectMessage = "Connection Request";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::CConnectProtocolStub : public IConnectProtocol
{
public:
    CConnectProtocolStub();

    // IConnectProtocol {
    virtual bool SendRequest(
        BryptIdentifier::SharedContainer const& spSourceIdentifier,
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        CMessageContext const& context) const override;
    // } IConnectProtocol 

    bool CalledOnce() const;

private:
    mutable std::uint32_t m_count;

};

//------------------------------------------------------------------------------------------------

class local::CPeerObserverStub : public IPeerObserver
{
public:
    explicit CPeerObserverStub(IPeerMediator* const mediator)
        : m_mediator(mediator)
        , m_spBryptPeer()
        , m_state(ConnectionState::Unknown)
    {
        m_mediator->RegisterObserver(this);
    }

    CPeerObserverStub(CPeerObserverStub&& other)
        : m_mediator(other.m_mediator)
        , m_spBryptPeer(std::move(other.m_spBryptPeer))
        , m_state(other.m_state)
    {
        m_mediator->RegisterObserver(this);
    }

    ~CPeerObserverStub()
    {
        m_mediator->UnpublishObserver(this);
    }

    // IPeerObserver {
    void HandlePeerStateChange(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        [[maybe_unused]] Endpoints::EndpointIdType identifier,
        [[maybe_unused]] Endpoints::TechnologyType technology,
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

    std::shared_ptr<CBryptPeer> GetBryptPeer() const
    {
        return m_spBryptPeer;
    }

    ConnectionState GetConnectionState() const
    {
        return m_state;
    }

private:
    IPeerMediator* m_mediator;
    std::shared_ptr<CBryptPeer> m_spBryptPeer;
    ConnectionState m_state;
};

//------------------------------------------------------------------------------------------------

class local::CMessageCollector : public IMessageSink
{
public:
    CMessageCollector();

    // IMessageSink {
    virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        std::string_view buffer) override;
        
    virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        Message::Buffer const& buffer) override;
    // }IMessageSink

};

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerDeclarationTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::ServerEntry);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DuplicatePeerDeclarationTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::ServerEntry);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::uint32_t(0));

    auto const optCheckRequest = manager.DeclareResolvingPeer(test::ServerEntry);
    EXPECT_FALSE(optCheckRequest);
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DeclaredPeerLinkTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    auto const optRequest = manager.DeclareResolvingPeer(test::ServerEntry);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::uint32_t(0));

    auto const spBryptPeer = manager.LinkPeer(*test::ClientIdentifier, test::ServerEntry);

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    ASSERT_TRUE(spBryptPeer);
    EXPECT_TRUE(spBryptPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spBryptPeer->RegisteredEndpointCount(), std::uint32_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, UndeclaredPeerLinkTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    auto const spBryptPeer = manager.LinkPeer(*test::ClientIdentifier);

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    ASSERT_TRUE(spBryptPeer);
    EXPECT_TRUE(spBryptPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spBryptPeer->RegisteredEndpointCount(), std::uint32_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, ExistingPeerLinkTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    auto const spFirstPeer = manager.LinkPeer(*test::ClientIdentifier);

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    spFirstPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    ASSERT_TRUE(spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    auto const loraIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    auto const spSecondPeer = manager.LinkPeer(*test::ClientIdentifier);
    spSecondPeer->RegisterEndpoint(
        loraIdentifier, Endpoints::TechnologyType::TCP, {}, "915:71");

    EXPECT_EQ(spSecondPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, DuplicateEqualSharedPeerLinkTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    auto const spFirstPeer = manager.LinkPeer(*test::ClientIdentifier);

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    spFirstPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    ASSERT_TRUE(spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(tcpIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    auto const loraIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    auto const spSecondPeer = manager.LinkPeer(*test::ClientIdentifier);
    spSecondPeer->RegisterEndpoint(
        loraIdentifier, Endpoints::TechnologyType::TCP, {}, "915:71");

    EXPECT_EQ(spSecondPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    auto const spThirdPeer = manager.LinkPeer(*test::ClientIdentifier);
    spThirdPeer->RegisterEndpoint(
        loraIdentifier, Endpoints::TechnologyType::TCP, {}, "915:71");

    EXPECT_EQ(spThirdPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerSingleEndpointDisconnectTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier);

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Endpoints::TechnologyType::TCP);

    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerMultipleEndpointDisconnectTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier);

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    manager.LinkPeer(*test::ClientIdentifier);

    auto const loraIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    spBryptPeer->RegisterEndpoint(
        loraIdentifier, Endpoints::TechnologyType::TCP, {}, "915:71");

    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Endpoints::TechnologyType::TCP);

    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    spBryptPeer->WithdrawEndpoint(loraIdentifier, Endpoints::TechnologyType::LoRa);

    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, PeerExchangeSetupTest)
{
    auto const upConnectProtocol = std::make_unique<local::CConnectProtocolStub>();
    auto const spMessageCollector = std::make_shared<local::CMessageCollector>();

    CPeerManager manager(test::ClientIdentifier, upConnectProtocol.get(), spMessageCollector);
    EXPECT_EQ(manager.ObservedPeerCount(), std::uint32_t(0));

    // Declare the client and server peers. 
    std::shared_ptr<CBryptPeer> spClientPeer;
    std::shared_ptr<CBryptPeer> spServerPeer;

    // Simulate an endpoint delcaring that it is attempting to resolve a peer at a
    // given uri. 
    auto const optRequest = manager.DeclareResolvingPeer(test::ServerEntry);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::uint32_t(0));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    // Simulate the server receiving the connection request. 
    spClientPeer = manager.LinkPeer(*test::ClientIdentifier);
    EXPECT_FALSE(spClientPeer->IsAuthorized());
    EXPECT_FALSE(spClientPeer->IsFlagged());
    EXPECT_EQ(manager.ObservedPeerCount(), std::uint32_t(1));

    // Create a mock endpoint identifier for the simulated endpoint the client has connected on. 
    auto const clientEndpoint = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    // Create a mock message context for messages passed through the client peer. 
    auto const clientContext = CMessageContext(clientEndpoint, Endpoints::TechnologyType::TCP);

    // Create a mock endpoint identifier for the simulated endpoint the server has responded to. 
    auto const serverEndpoint = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    // Create a mock message context for messages passed through the server peer. 
    auto const serverContext = CMessageContext(serverEndpoint, Endpoints::TechnologyType::TCP);

    // Simulate the server's endpoint registering itself to the given client peer. 
    spClientPeer->RegisterEndpoint(
        clientContext.GetEndpointIdentifier(), clientContext.GetEndpointTechnology(),
        [&spServerPeer, &serverContext] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            EXPECT_TRUE(spServerPeer->ScheduleReceive(serverContext, message));
            return true;
        });

    // In practice the client would receive a response from the server before linking a peer. 
    // However, we need to create a peer to properly handle the exchange on the stack. 
    spServerPeer = manager.LinkPeer(*test::ServerIdentifier, test::ServerEntry);
    EXPECT_FALSE(spServerPeer->IsAuthorized());
    EXPECT_FALSE(spServerPeer->IsFlagged());
    EXPECT_EQ(manager.ObservedPeerCount(), std::uint32_t(2));

    // Simulate the clients's endpoint registering itself to the given server peer. 
    spServerPeer->RegisterEndpoint(
        serverContext.GetEndpointIdentifier(), serverContext.GetEndpointTechnology(),
        [&spClientPeer, &clientContext] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            EXPECT_TRUE(spClientPeer->ScheduleReceive(clientContext, message));
            return true;
        });

    // Cause the key exchange setup by the peer manager to occur on the stack. 
    EXPECT_TRUE(spClientPeer->ScheduleReceive(clientContext, *optRequest));

    // Verify the results of the key exchange
    EXPECT_TRUE(upConnectProtocol->CalledOnce());
    EXPECT_TRUE(spClientPeer->IsAuthorized());
    EXPECT_TRUE(spServerPeer->IsAuthorized());
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, SingleForEachIdentiferCacheTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);

    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier);

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    manager.ForEachCachedIdentifier(
        [&spBryptPeer] (auto const& spCheckIdentifier) -> CallbackIteration
        {
            EXPECT_EQ(spCheckIdentifier, spBryptPeer->GetBryptIdentifier());
            EXPECT_EQ(*spCheckIdentifier, *spBryptPeer->GetBryptIdentifier());
            return CallbackIteration::Continue;
        });

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Endpoints::TechnologyType::TCP);

    std::uint32_t iterations = 0;
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));
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
    CPeerManager manager(test::ServerIdentifier, nullptr);

    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    std::uint32_t const iterations = 1000;

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    for (std::uint32_t idx = 0; idx < iterations; ++idx) {
        auto spBryptPeer = manager.LinkPeer(
            BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
        spBryptPeer->RegisterEndpoint(
            tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);
        if (distribution(generator)) {
            spBryptPeer->WithdrawEndpoint(tcpIdentifier, Endpoints::TechnologyType::TCP);
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
    CPeerManager manager(test::ServerIdentifier, nullptr);

    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    std::uint32_t const iterations = 1000;
    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    for (std::uint32_t idx = 0; idx < iterations; ++idx) {
        auto spBryptPeer = manager.LinkPeer(
            BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
        spBryptPeer->RegisterEndpoint(
            tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);
        if (distribution(generator)) {
            spBryptPeer->WithdrawEndpoint(tcpIdentifier, Endpoints::TechnologyType::TCP);
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
    CPeerManager manager(test::ServerIdentifier, nullptr);
    local::CPeerObserverStub observer(&manager);

    EXPECT_FALSE(observer.GetBryptPeer());
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Unknown);

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    EXPECT_EQ(observer.GetBryptPeer(), spBryptPeer);
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Connected);

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Endpoints::TechnologyType::TCP);

    EXPECT_FALSE(observer.GetBryptPeer());
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Disconnected);
}

//------------------------------------------------------------------------------------------------

TEST(PeerManagerSuite, MultipleObserverTest)
{
    CPeerManager manager(test::ServerIdentifier, nullptr);

    std::vector<local::CPeerObserverStub> observers;
    for (std::uint32_t idx = 0; idx < 12; ++idx) {
        observers.emplace_back(local::CPeerObserverStub(&manager));
    }

    for (auto const& observer: observers) {
        EXPECT_FALSE(observer.GetBryptPeer());
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Unknown);
    }

    auto const tcpIdentifier = CEndpointIdentifierGenerator::Instance()
        .GetEndpointIdentifier();

    auto spBryptPeer = manager.LinkPeer(*test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        tcpIdentifier, Endpoints::TechnologyType::TCP, {}, test::ServerEntry);

    for (auto const& observer: observers) {
        EXPECT_EQ(observer.GetBryptPeer(), spBryptPeer);
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Connected);
    }

    spBryptPeer->WithdrawEndpoint(tcpIdentifier, Endpoints::TechnologyType::TCP);

    for (auto const& observer: observers) {
        EXPECT_FALSE(observer.GetBryptPeer());
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Disconnected);
    }
}

//------------------------------------------------------------------------------------------------

local::CConnectProtocolStub::CConnectProtocolStub()
    : m_count(0)
{
}

//------------------------------------------------------------------------------------------------

bool local::CConnectProtocolStub::SendRequest(
    [[maybe_unused]] BryptIdentifier::SharedContainer const& spSourceIdentifier,
    [[maybe_unused]] std::shared_ptr<CBryptPeer> const& spBryptPeer,
    [[maybe_unused]] CMessageContext const& context) const
{
    ++m_count;
    return true;
}

//------------------------------------------------------------------------------------------------

bool local::CConnectProtocolStub::CalledOnce() const
{
    return (m_count == 1);
}

//------------------------------------------------------------------------------------------------

local::CMessageCollector::CMessageCollector()
{
}

//------------------------------------------------------------------------------------------------

bool local::CMessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    [[maybe_unused]] CMessageContext const& context,
    [[maybe_unused]] std::string_view buffer)
{
    return true;
}

//------------------------------------------------------------------------------------------------

bool local::CMessageCollector::CollectMessage(
    [[maybe_unused]] std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    [[maybe_unused]] CMessageContext const& context,
    [[maybe_unused]] Message::Buffer const& buffer)
{
    return false;
}

//------------------------------------------------------------------------------------------------
