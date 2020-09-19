//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Components/BryptPeer/PeerManager.hpp"
#include "../../Components/Endpoints/ConnectionState.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
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

class CPeerObserverStub;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
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

TEST(CPeerManagerSuite, NewPeerLinkTest)
{
    CPeerManager manager;
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    auto const spBryptPeer = manager.LinkPeer(test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");

    ASSERT_TRUE(spBryptPeer);
    EXPECT_TRUE(spBryptPeer->IsEndpointRegistered(tcpEndpointIdentifier));
    EXPECT_EQ(spBryptPeer->RegisteredEndpointCount(), std::uint32_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(CPeerManagerSuite, ExistingPeerLinkTest)
{
    CPeerManager manager;
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    auto const spFirstPeer = manager.LinkPeer(test::ClientIdentifier);
    spFirstPeer->RegisterEndpoint(
        tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");

    ASSERT_TRUE(spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(tcpEndpointIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    Endpoints::EndpointIdType const loraEndpointIdentifier = rand();
    auto const spSecondPeer = manager.LinkPeer(test::ClientIdentifier);
    spSecondPeer->RegisterEndpoint(
        loraEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "915:71");

    EXPECT_EQ(spSecondPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraEndpointIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

}

//------------------------------------------------------------------------------------------------

TEST(CPeerManagerSuite, DuplicateEqualSharedPeerLinkTest)
{
    CPeerManager manager;
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    auto const spFirstPeer = manager.LinkPeer(test::ClientIdentifier);
    spFirstPeer->RegisterEndpoint(
        tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");

    ASSERT_TRUE(spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(tcpEndpointIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(1));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    Endpoints::EndpointIdType const loraEndpointIdentifier = rand();
    auto const spSecondPeer = manager.LinkPeer(test::ClientIdentifier);
    spSecondPeer->RegisterEndpoint(
        loraEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "915:71");

    EXPECT_EQ(spSecondPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraEndpointIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    auto const spThirdPeer = manager.LinkPeer(test::ClientIdentifier);
    spThirdPeer->RegisterEndpoint(
        loraEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "915:71");

    EXPECT_EQ(spThirdPeer, spFirstPeer);
    EXPECT_TRUE(spFirstPeer->IsEndpointRegistered(loraEndpointIdentifier));
    EXPECT_EQ(spFirstPeer->RegisteredEndpointCount(), std::uint32_t(2));
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(CPeerManagerSuite, PeerSingleEndpointDisconnectTest)
{
    CPeerManager manager;
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    auto spBryptPeer = manager.LinkPeer(test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    spBryptPeer->WithdrawEndpoint(tcpEndpointIdentifier, Endpoints::TechnologyType::TCP);

    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CPeerManagerSuite, PeerMultipleEndpointDisconnectTest)
{
    CPeerManager manager;
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));

    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    auto spBryptPeer = manager.LinkPeer(test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    Endpoints::EndpointIdType const loraEndpointIdentifier = rand();
    manager.LinkPeer(test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        loraEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "915:71");

    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    spBryptPeer->WithdrawEndpoint(tcpEndpointIdentifier, Endpoints::TechnologyType::TCP);

    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    spBryptPeer->WithdrawEndpoint(loraEndpointIdentifier, Endpoints::TechnologyType::LoRa);

    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CPeerManagerSuite, SingleForEachIdentiferCacheTest)
{
    CPeerManager manager;

    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    auto spBryptPeer = manager.LinkPeer(test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");

    ASSERT_TRUE(spBryptPeer);
    EXPECT_EQ(manager.ActivePeerCount(), std::uint32_t(1));

    manager.ForEachCachedIdentifier(
        [&spBryptPeer] (auto const& spCheckIdentifier) -> CallbackIteration
        {
            EXPECT_EQ(spCheckIdentifier, spBryptPeer->GetBryptIdentifier());
            EXPECT_EQ(*spCheckIdentifier, *spBryptPeer->GetBryptIdentifier());
            return CallbackIteration::Continue;
        });

    spBryptPeer->WithdrawEndpoint(tcpEndpointIdentifier, Endpoints::TechnologyType::TCP);

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

TEST(CPeerManagerSuite, MultipleForEachIdentiferCacheTest)
{
    CPeerManager manager;

    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    std::uint32_t const iterations = 1000;
    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    for (std::uint32_t idx = 0; idx < iterations; ++idx) {
        auto spBryptPeer = manager.LinkPeer(
            BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
        spBryptPeer->RegisterEndpoint(
            tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");
        if (distribution(generator)) {
            spBryptPeer->WithdrawEndpoint(tcpEndpointIdentifier, Endpoints::TechnologyType::TCP);
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

TEST(CPeerManagerSuite, PeerCountTest)
{
    CPeerManager manager;

    std::random_device device;
    std::mt19937 generator(device());
    std::bernoulli_distribution distribution(0.33);

    std::uint32_t disconnected = 0;
    std::uint32_t const iterations = 1000;
    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    for (std::uint32_t idx = 0; idx < iterations; ++idx) {
        auto spBryptPeer = manager.LinkPeer(
            BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
        spBryptPeer->RegisterEndpoint(
            tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");
        if (distribution(generator)) {
            spBryptPeer->WithdrawEndpoint(tcpEndpointIdentifier, Endpoints::TechnologyType::TCP);
            ++disconnected;
        }
    }
    EXPECT_EQ(manager.ActivePeerCount(), iterations - disconnected);
    EXPECT_EQ(manager.InactivePeerCount(), disconnected);
    EXPECT_EQ(manager.ObservedPeerCount(), iterations);
}

//------------------------------------------------------------------------------------------------

TEST(CPeerManagerSuite, SingleObserverTest)
{
    CPeerManager manager;
    local::CPeerObserverStub observer(&manager);

    EXPECT_FALSE(observer.GetBryptPeer());
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Unknown);

    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    auto spBryptPeer = manager.LinkPeer(test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");

    EXPECT_EQ(observer.GetBryptPeer(), spBryptPeer);
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Connected);

    spBryptPeer->WithdrawEndpoint(tcpEndpointIdentifier, Endpoints::TechnologyType::TCP);

    EXPECT_FALSE(observer.GetBryptPeer());
    EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Disconnected);
}

//------------------------------------------------------------------------------------------------

TEST(CPeerManagerSuite, MultipleObserverTest)
{
    CPeerManager manager;

    std::vector<local::CPeerObserverStub> observers;
    for (std::uint32_t idx = 0; idx < 12; ++idx) {
        observers.emplace_back(local::CPeerObserverStub(&manager));
    }

    for (auto const& observer: observers) {
        EXPECT_FALSE(observer.GetBryptPeer());
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Unknown);
    }

    Endpoints::EndpointIdType const tcpEndpointIdentifier = rand();
    auto spBryptPeer = manager.LinkPeer(test::ClientIdentifier);
    spBryptPeer->RegisterEndpoint(
        tcpEndpointIdentifier, Endpoints::TechnologyType::TCP, {}, "127.0.0.1:35216");

    for (auto const& observer: observers) {
        EXPECT_EQ(observer.GetBryptPeer(), spBryptPeer);
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Connected);
    }

    spBryptPeer->WithdrawEndpoint(tcpEndpointIdentifier, Endpoints::TechnologyType::TCP);

    for (auto const& observer: observers) {
        EXPECT_FALSE(observer.GetBryptPeer());
        EXPECT_EQ(observer.GetConnectionState(), ConnectionState::Disconnected);
    }
}

//------------------------------------------------------------------------------------------------
