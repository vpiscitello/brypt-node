//------------------------------------------------------------------------------------------------
#include "../../Components/Command/CommandDefinitions.hpp"
#include "../../Components/Endpoints/ConnectionState.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/EndpointManager.hpp"
#include "../../Components/Endpoints/Peer.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Interfaces/PeerCache.hpp"
#include "../../Interfaces/PeerMediator.hpp"
#include "../../Interfaces/PeerObserver.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

class CPeerObserverStub;
class CPeerCacheStub;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr NodeUtils::NodeIdType ServerId = 0x12345678;
constexpr NodeUtils::NodeIdType ClientId = 0x77777777;
constexpr std::string_view TechnologyName = "Direct";
constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::Direct;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";
constexpr std::string_view ClientEntry = "127.0.0.1:35217";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::CPeerObserverStub : public IPeerObserver
{
public:
    explicit CPeerObserverStub(IPeerMediator* const mediator)
        : m_mediator(mediator)
        , m_upPeer()
        , m_state(ConnectionState::Unknown)
    {
        m_mediator->RegisterObserver(this);
    }

    CPeerObserverStub(CPeerObserverStub&& other)
        : m_mediator(other.m_mediator)
        , m_upPeer(std::move(other.m_upPeer))
        , m_state(other.m_state)
    {
        m_mediator->RegisterObserver(this);
    }

    ~CPeerObserverStub()
    {
        m_mediator->UnpublishObserver(this);
    }

    // IPeerObserver {
    void HandlePeerConnectionStateChange(
        CPeer const& peer,
        ConnectionState change) override
    {
        m_state = change;
        switch(m_state) {
            case ConnectionState::Connected: {
                m_upPeer = std::make_unique<CPeer>(peer);
            } break;
            case ConnectionState::Disconnected: {
                m_upPeer.reset();
            } break;
            // Not currently testing other connection states for the observer
            default: assert(false); break;
        }
    }
    // } IPeerObserver

    std::optional<CPeer> GetPeer() const
    {
        if (!m_upPeer) {
            return {};
        }
        return *m_upPeer;
    }

    ConnectionState GetConnectionState() const
    {
        return m_state;
    }

private:
    IPeerMediator* m_mediator;
    std::unique_ptr<CPeer> m_upPeer;
    ConnectionState m_state;
};

//------------------------------------------------------------------------------------------------

class local::CPeerCacheStub : public IPeerCache
{
public:
    CPeerCacheStub()
        : m_endpoints()
    {
    }

    void AddPeer(CPeer const& peer)
    {
        auto& spPeersMap = m_endpoints[peer.GetTechnologyType()]; 
        if (!spPeersMap) {
            spPeersMap = std::make_shared<CPeerPersistor::PeersMap>();
        }
        spPeersMap->emplace(peer.GetEntry(), peer);
    }

    // IPeerCache {
    bool ForEachCachedEndpoint(
        [[maybe_unused]] AllEndpointReadFunction const& readFunction) const override
    {
        return false;  
    }

    bool ForEachCachedPeer(
        [[maybe_unused]] AllEndpointPeersReadFunction const& readFunction,
        [[maybe_unused]] AllEndpointPeersErrorFunction const& errorFunction) const override
    {
        return false;
    }

    bool ForEachCachedPeer(
        Endpoints::TechnologyType technology,
        OneEndpointPeersReadFunction const& readFunction) const override
    {
        auto const itr = m_endpoints.find(technology);
        if (itr == m_endpoints.end()) {
            return false;
        }   

        auto const& [key, spPeersMap] = *itr;
        for (auto const& [id, peer]: *spPeersMap) {
            auto const result = readFunction(peer);
            if (result != CallbackIteration::Continue) {
                break;
            }
        }

        return true;
    }

    std::uint32_t CachedEndpointsCount() const override
    {
        return 0;
    }

    std::uint32_t CachedPeersCount() const override
    {
        return 0;
    }

    std::uint32_t CachedPeersCount(
        [[maybe_unused]] Endpoints::TechnologyType technology) const override
    {
        return 0;
    }
    // } IPeerCache

private:
    CPeerPersistor::EndpointPeersMap m_endpoints;

};

//------------------------------------------------------------------------------------------------

TEST(CEndpointManagerSuite, SingleObserverTest)
{
    CEndpointManager manager;
    local::CPeerObserverStub observer(&manager);

    auto const optInitialCheckPeer = observer.GetPeer();
    ConnectionState const checkInitialState = observer.GetConnectionState();
    ASSERT_FALSE(optInitialCheckPeer);
    EXPECT_EQ(checkInitialState, ConnectionState::Unknown);

    CPeer const peer(
        test::ClientId,
        test::TechnologyType,
        test::ClientEntry);

    manager.ForwardPeerConnectionStateChange(peer, ConnectionState::Connected);
    auto const optCheckConnectedPeer = observer.GetPeer();
    ConnectionState const checkConnectedState = observer.GetConnectionState();

    ASSERT_TRUE(optCheckConnectedPeer);
    EXPECT_EQ(optCheckConnectedPeer->GetNodeId(), peer.GetNodeId());
    EXPECT_EQ(optCheckConnectedPeer->GetTechnologyType(), peer.GetTechnologyType());
    EXPECT_EQ(optCheckConnectedPeer->GetEntry(), peer.GetEntry());
    EXPECT_EQ(optCheckConnectedPeer->GetLocation(), peer.GetLocation());
    EXPECT_EQ(checkConnectedState, ConnectionState::Connected);

    manager.ForwardPeerConnectionStateChange(peer, ConnectionState::Disconnected);
    auto const optCheckDisonnectedPeer = observer.GetPeer();
    ConnectionState const checkDisonnectedState = observer.GetConnectionState();
    ASSERT_FALSE(optCheckDisonnectedPeer);
    EXPECT_EQ(checkDisonnectedState, ConnectionState::Disconnected);
}

//------------------------------------------------------------------------------------------------

TEST(CEndpointManagerSuite, MultipleObserverTest)
{
    CEndpointManager manager;

    std::vector<local::CPeerObserverStub> observers;
    for (std::uint32_t idx = 0; idx < 12; ++idx) {
        observers.emplace_back(local::CPeerObserverStub(&manager));
    }

    for (auto const& observer: observers) {
        auto const optInitialCheckPeer = observer.GetPeer();
        ConnectionState const checkInitialState = observer.GetConnectionState();
        ASSERT_FALSE(optInitialCheckPeer);
        EXPECT_EQ(checkInitialState, ConnectionState::Unknown);
    }

    CPeer const peer(
        test::ClientId,
        test::TechnologyType,
        test::ClientEntry);

    manager.ForwardPeerConnectionStateChange(peer, ConnectionState::Connected);

    for (auto const& observer: observers) {
        auto const optCheckConnectedPeer = observer.GetPeer();
        ConnectionState const checkConnectedState = observer.GetConnectionState();

        ASSERT_TRUE(optCheckConnectedPeer);
        EXPECT_EQ(optCheckConnectedPeer->GetNodeId(), peer.GetNodeId());
        EXPECT_EQ(optCheckConnectedPeer->GetTechnologyType(), peer.GetTechnologyType());
        EXPECT_EQ(optCheckConnectedPeer->GetEntry(), peer.GetEntry());
        EXPECT_EQ(optCheckConnectedPeer->GetLocation(), peer.GetLocation());
        EXPECT_EQ(checkConnectedState, ConnectionState::Connected);
    }

    manager.ForwardPeerConnectionStateChange(peer, ConnectionState::Disconnected);

    for (auto const& observer: observers) {
        auto const optCheckDisonnectedPeer = observer.GetPeer();
        ConnectionState const checkDisonnectedState = observer.GetConnectionState();
        ASSERT_FALSE(optCheckDisonnectedPeer);
        EXPECT_EQ(checkDisonnectedState, ConnectionState::Disconnected);
    }
}

//------------------------------------------------------------------------------------------------

TEST(CEndpointManagerSuite, EndpointStartupTest)
{
    auto upEndpointManager = std::make_unique<CEndpointManager>();

    Configuration::EndpointConfigurations configurations;
    Configuration::TEndpointOptions options(
        Endpoints::TechnologyType::TCP,
        test::Interface,
        test::ServerBinding);
    configurations.emplace_back(options);
    
    local::CPeerCacheStub cache;
    CPeer const peer(test::ClientId, Endpoints::TechnologyType::TCP, test::ClientEntry);
    cache.AddPeer(peer);

    upEndpointManager->Initialize(test::ServerId, nullptr, configurations, &cache);
    std::uint32_t const initialActiveEndpoints = upEndpointManager->ActiveEndpointCount();
    std::uint32_t const initialActiveTechnologiesCount = upEndpointManager->ActiveTechnologyCount();
    EXPECT_EQ(initialActiveEndpoints, std::uint32_t(0));
    EXPECT_EQ(initialActiveTechnologiesCount, std::uint32_t(0));

    upEndpointManager->Startup();
    std::uint32_t const startupActiveEndpoints = upEndpointManager->ActiveEndpointCount();
    std::uint32_t const startupActiveTechnologiesCount = upEndpointManager->ActiveTechnologyCount();
    EXPECT_GT(startupActiveEndpoints, std::uint32_t(0));
    EXPECT_EQ(startupActiveTechnologiesCount, configurations.size());

    upEndpointManager->Shutdown();
    std::uint32_t const shutdownActiveEndpoints = upEndpointManager->ActiveEndpointCount();
    std::uint32_t const shutdownActiveTechnologiesCount = upEndpointManager->ActiveTechnologyCount();
    EXPECT_EQ(shutdownActiveEndpoints, std::uint32_t(0));
    EXPECT_EQ(shutdownActiveTechnologiesCount, std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------
