//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/RuntimeContext.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Network/Protocol.hpp"
#include "Interfaces/BootstrapCache.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class BootstrapCacheStub;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

constexpr Network::Protocol ProtocolType = Network::Protocol::TCP;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::BootstrapCacheStub : public IBootstrapCache
{
public:
    BootstrapCacheStub() : m_protocols() {}

    void AddBootstrap(Network::RemoteAddress const& bootstrap)
    {
        auto& upBootstrapSet = m_protocols[bootstrap.GetProtocol()]; 
        if (!upBootstrapSet) { upBootstrapSet = std::make_unique<PeerPersistor::BootstrapSet>(); }
        upBootstrapSet->emplace(bootstrap);
    }

    // IBootstrapCache {
    bool ForEachCachedBootstrap(
        [[maybe_unused]] AllProtocolsReadFunction const& readFunction,
        [[maybe_unused]] AllProtocolsErrorFunction const& errorFunction) const override
    {
        return false;
    }

    bool ForEachCachedBootstrap(Network::Protocol protocol, OneProtocolReadFunction const& readFunction) const override
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

    std::size_t CachedBootstrapCount() const override { return 0; }
    std::size_t CachedBootstrapCount([[maybe_unused]] Network::Protocol protocol) const override { return 0; }
    // } IBootstrapCache

private:
    PeerPersistor::ProtocolMap m_protocols;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkManagerSuite, EndpointStartupTest)
{
    Configuration::EndpointsSet endpoints;
    Configuration::EndpointOptions options(Network::Protocol::TCP, test::Interface, test::ServerBinding);
    ASSERT_TRUE(options.Initialize());
    endpoints.emplace_back(options);
    
    auto const spPeerCache = std::make_shared<local::BootstrapCacheStub>();
    Network::RemoteAddress address(test::ProtocolType, test::ServerEntry, true);
    spPeerCache->AddBootstrap(address);

    auto const spPublisher = std::make_shared<Event::Publisher>();
    auto const upNetworkManager = std::make_unique<Network::Manager>(
        endpoints, spPublisher, nullptr, spPeerCache.get(), RuntimeContext::Foreground);
        
    std::size_t const initialActiveEndpoints = upNetworkManager->ActiveEndpointCount();
    std::size_t const initialActiveProtocolsCount = upNetworkManager->ActiveProtocolCount();
    EXPECT_EQ(initialActiveEndpoints, std::size_t(0));
    EXPECT_EQ(initialActiveProtocolsCount, std::size_t(0));

    upNetworkManager->Startup();
    std::size_t const startupActiveEndpoints = upNetworkManager->ActiveEndpointCount();
    std::size_t const startupActiveProtocolsCount = upNetworkManager->ActiveProtocolCount();
    EXPECT_GT(startupActiveEndpoints, std::size_t(0));
    EXPECT_EQ(startupActiveProtocolsCount, endpoints.size());

    upNetworkManager->Shutdown();
    std::size_t const shutdownActiveEndpoints = upNetworkManager->ActiveEndpointCount();
    std::size_t const shutdownActiveProtocolsCount = upNetworkManager->ActiveProtocolCount();
    EXPECT_EQ(shutdownActiveEndpoints, std::size_t(0));
    EXPECT_EQ(shutdownActiveProtocolsCount, std::size_t(0));
}

//----------------------------------------------------------------------------------------------------------------------
