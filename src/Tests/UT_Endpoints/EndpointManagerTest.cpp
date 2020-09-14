//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Components/Command/CommandDefinitions.hpp"
#include "../../Components/Endpoints/ConnectionState.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/EndpointManager.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Interfaces/BootstrapCache.hpp"
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
class CBootstrapCacheStub;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

auto const spClientIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());
auto const spServerIdentifier = std::make_shared<BryptIdentifier::CContainer>(
    BryptIdentifier::Generate());

constexpr std::string_view TechnologyName = "Direct";
constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::Direct;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35222";
constexpr std::string_view ClientBinding = "*:35223";
constexpr std::string_view ServerEntry = "127.0.0.1:35222";
constexpr std::string_view ClientEntry = "127.0.0.1:35223";

//------------------------------------------------------------------------------------------------
} // test namespace
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
    void HandleConnectionStateChange(
        [[maybe_unused]] Endpoints::TechnologyType technology,
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
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
            default: assert(false); break;
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

class local::CBootstrapCacheStub : public IBootstrapCache
{
public:
    CBootstrapCacheStub()
        : m_endpoints()
    {
    }

    void AddBootstrap(Endpoints::TechnologyType technology, std::string_view bootstrap)
    {
        auto& upBootstrapSet = m_endpoints[technology]; 
        if (!upBootstrapSet) {
            upBootstrapSet = std::make_unique<CPeerPersistor::BootstrapSet>();
        }
        upBootstrapSet->emplace(bootstrap);
    }

    // IBootstrapCache {

    bool ForEachCachedBootstrap(
        [[maybe_unused]] AllEndpointBootstrapReadFunction const& readFunction,
        [[maybe_unused]] AllEndpointBootstrapErrorFunction const& errorFunction) const override
    {
        return false;
    }

    bool ForEachCachedBootstrap(
        Endpoints::TechnologyType technology,
        OneEndpointBootstrapReadFunction const& readFunction) const override
    {
        auto const itr = m_endpoints.find(technology);
        if (itr == m_endpoints.end()) {
            return false;
        }   

        auto const& [key, spBootstrapSet] = *itr;
        for (auto const& bootsrap: *spBootstrapSet) {
            auto const result = readFunction(bootsrap);
            if (result != CallbackIteration::Continue) {
                break;
            }
        }

        return true;
    }

    std::uint32_t CachedBootstrapCount() const override
    {
        return 0;
    }

    std::uint32_t CachedBootstrapCount(
        [[maybe_unused]] Endpoints::TechnologyType technology) const override
    {
        return 0;
    }
    // } IBootstrapCache

private:
    CPeerPersistor::EndpointBootstrapMap m_endpoints;

};

//------------------------------------------------------------------------------------------------

TEST(CEndpointManagerSuite, SingleObserverTest)
{
    CEndpointManager manager;
    local::CPeerObserverStub observer(&manager);

    auto const optInitialCheckPeer = observer.GetBryptPeer();
    ConnectionState const checkInitialState = observer.GetConnectionState();
    ASSERT_FALSE(optInitialCheckPeer);
    EXPECT_EQ(checkInitialState, ConnectionState::Unknown);

    auto const spBryptPeer = std::make_shared<CBryptPeer>(*test::spClientIdentifier);
    spBryptPeer->RegisterEndpointConnection(
        rand(), test::TechnologyType, {}, test::ClientEntry);

    manager.ForwardConnectionStateChange(
        test::TechnologyType, spBryptPeer, ConnectionState::Connected);
    auto const spCheckBryptPeer = observer.GetBryptPeer();
    ConnectionState const checkConnectedState = observer.GetConnectionState();

    ASSERT_TRUE(spCheckBryptPeer);
    EXPECT_EQ(spCheckBryptPeer->GetBryptIdentifier(), spBryptPeer->GetBryptIdentifier());
    EXPECT_EQ(checkConnectedState, ConnectionState::Connected);

    manager.ForwardConnectionStateChange(
        test::TechnologyType, spBryptPeer, ConnectionState::Disconnected);
    auto const optCheckDisonnectedPeer = observer.GetBryptPeer();
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
        auto const optInitialCheckPeer = observer.GetBryptPeer();
        ConnectionState const checkInitialState = observer.GetConnectionState();
        ASSERT_FALSE(optInitialCheckPeer);
        EXPECT_EQ(checkInitialState, ConnectionState::Unknown);
    }

    auto const spBryptPeer = std::make_shared<CBryptPeer>(*test::spClientIdentifier);
    spBryptPeer->RegisterEndpointConnection(
        rand(), test::TechnologyType, {}, test::ClientEntry);

    manager.ForwardConnectionStateChange(
        test::TechnologyType, spBryptPeer, ConnectionState::Connected);

    for (auto const& observer: observers) {
        auto const spCheckBryptPeer = observer.GetBryptPeer();
        ConnectionState const checkConnectedState = observer.GetConnectionState();

        ASSERT_TRUE(spCheckBryptPeer);
        EXPECT_EQ(spCheckBryptPeer->GetBryptIdentifier(), spBryptPeer->GetBryptIdentifier());
        EXPECT_EQ(checkConnectedState, ConnectionState::Connected);
    }

    manager.ForwardConnectionStateChange(
        test::TechnologyType, spBryptPeer, ConnectionState::Disconnected);

    for (auto const& observer: observers) {
        auto const optCheckDisonnectedPeer = observer.GetBryptPeer();
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
    
    local::CBootstrapCacheStub cache;
    cache.AddBootstrap(test::TechnologyType, test::ClientEntry);

    upEndpointManager->Initialize(test::spServerIdentifier, nullptr, configurations, &cache);
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
