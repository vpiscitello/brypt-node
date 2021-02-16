//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/EndpointManager.hpp"
#include "Components/Network/Protocol.hpp"
#include "Interfaces/BootstrapCache.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

class BootstrapCacheStub;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr Network::Protocol ProtocolType = Network::Protocol::TCP;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";

//------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//------------------------------------------------------------------------------------------------

class local::BootstrapCacheStub : public IBootstrapCache
{
public:
    BootstrapCacheStub()
        : m_protocols()
    {
    }

    void AddBootstrap(Network::RemoteAddress const& bootstrap)
    {
        auto& upBootstrapSet = m_protocols[bootstrap.GetProtocol()]; 
        if (!upBootstrapSet) {
            upBootstrapSet = std::make_unique<PeerPersistor::BootstrapSet>();
        }
        upBootstrapSet->emplace(bootstrap);
    }

    // IBootstrapCache {
    bool ForEachCachedBootstrap(
        [[maybe_unused]] AllProtocolsReadFunction const& readFunction,
        [[maybe_unused]] AllProtocolsErrorFunction const& errorFunction) const override
    {
        return false;
    }

    bool ForEachCachedBootstrap(
        Network::Protocol protocol,
        OneProtocolReadFunction const& readFunction) const override
    {
        auto const itr = m_protocols.find(protocol);
        if (itr == m_protocols.end()) { return false; }   

        auto const& [key, spBootstrapSet] = *itr;
        for (auto const& bootsrap: *spBootstrapSet) {
            auto const result = readFunction(bootsrap);
            if (result != CallbackIteration::Continue) { break; }
        }

        return true;
    }

    std::size_t CachedBootstrapCount() const override
    {
        return 0;
    }

    std::size_t CachedBootstrapCount(
        [[maybe_unused]] Network::Protocol protocol) const override
    {
        return 0;
    }
    // } IBootstrapCache

private:
    PeerPersistor::ProtocolMap m_protocols;
};

//------------------------------------------------------------------------------------------------

TEST(EndpointManagerSuite, EndpointStartupTest)
{
    Configuration::EndpointConfigurations configurations;
    Configuration::EndpointOptions options(
        Network::Protocol::TCP,
        test::Interface,
        test::ServerBinding);
    ASSERT_TRUE(options.Initialize());
    configurations.emplace_back(options);
    
    auto const spPeerCache = std::make_shared<local::BootstrapCacheStub>();
    Network::RemoteAddress address(test::ProtocolType, test::ServerEntry, true);
    spPeerCache->AddBootstrap(address);

    auto upEndpointManager = std::make_unique<EndpointManager>(
        configurations, nullptr, spPeerCache.get());
        
    std::size_t const initialActiveEndpoints = upEndpointManager->ActiveEndpointCount();
    std::size_t const initialActiveProtocolsCount = upEndpointManager->ActiveProtocolCount();
    EXPECT_EQ(initialActiveEndpoints, std::size_t(0));
    EXPECT_EQ(initialActiveProtocolsCount, std::size_t(0));

    upEndpointManager->Startup();
    std::size_t const startupActiveEndpoints = upEndpointManager->ActiveEndpointCount();
    std::size_t const startupActiveProtocolsCount = upEndpointManager->ActiveProtocolCount();
    EXPECT_GT(startupActiveEndpoints, std::size_t(0));
    EXPECT_EQ(startupActiveProtocolsCount, configurations.size());

    upEndpointManager->Shutdown();
    std::size_t const shutdownActiveEndpoints = upEndpointManager->ActiveEndpointCount();
    std::size_t const shutdownActiveProtocolsCount = upEndpointManager->ActiveProtocolCount();
    EXPECT_EQ(shutdownActiveEndpoints, std::size_t(0));
    EXPECT_EQ(shutdownActiveProtocolsCount, std::size_t(0));
}

//------------------------------------------------------------------------------------------------
