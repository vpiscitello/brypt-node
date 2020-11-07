//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/EndpointManager.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Interfaces/BootstrapCache.hpp"
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

class CBootstrapCacheStub;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

auto const spBryptIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
    BryptIdentifier::Generate());

constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::TCP;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";

//------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
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

TEST(CEndpointManagerSuite, EndpointStartupTest)
{
    Configuration::EndpointConfigurations configurations;
    Configuration::TEndpointOptions options(
        Endpoints::TechnologyType::TCP,
        test::Interface,
        test::ServerBinding);
    configurations.emplace_back(options);
    
    auto const spPeerCache = std::make_shared<local::CBootstrapCacheStub>();
    spPeerCache->AddBootstrap(test::TechnologyType, test::ServerEntry);

    auto upEndpointManager = std::make_unique<CEndpointManager>(
        configurations, test::spBryptIdentifier, nullptr, spPeerCache);
        
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
