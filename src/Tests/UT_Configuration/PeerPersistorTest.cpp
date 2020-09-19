//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr Endpoints::EndpointIdType EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType PeerTechnology = Endpoints::TechnologyType::TCP;
constexpr std::string_view NewBootstrapEntry = "127.0.0.1:35220";

constexpr std::string_view TcpBootstrapEntry = "127.0.0.1:35216";
constexpr std::string_view DirectBootstrapEntry = "127.0.0.1:35217";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, GeneratePeersFilepathTest)
{
    auto const filepath = Configuration::GetDefaultPeersFilepath();
    EXPECT_TRUE(filepath.has_parent_path());
    EXPECT_TRUE(filepath.is_absolute());
    auto const found = filepath.string().find(Configuration::DefaultBryptFolder);
    EXPECT_NE(found, std::string::npos);
    EXPECT_EQ(filepath.filename(), Configuration::DefaultKnownPeersFilename);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, DefualtBootstrapTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/default-peers.json";

    Configuration::EndpointConfigurations configurations;

    Configuration::TEndpointOptions tcpOptions;
    tcpOptions.type = Endpoints::TechnologyType::TCP;
    tcpOptions.bootstrap = test::TcpBootstrapEntry;
    configurations.emplace_back(tcpOptions);

    Configuration::TEndpointOptions directOptions;
    directOptions.type = Endpoints::TechnologyType::Direct;
    directOptions.bootstrap = test::DirectBootstrapEntry;
    configurations.emplace_back(directOptions);

    CPeerPersistor persistor(filepath.c_str(), configurations);
    auto const bParsed = persistor.FetchBootstraps();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedBootstrapCount(), std::uint32_t(2));
    EXPECT_EQ(persistor.CachedBootstrapCount(Endpoints::TechnologyType::TCP), std::uint32_t(1));
    EXPECT_EQ(persistor.CachedBootstrapCount(Endpoints::TechnologyType::Direct), std::uint32_t(1));

    CPeerPersistor checkPersistor(filepath.c_str(), configurations);
    auto const bCheckParsed = checkPersistor.FetchBootstraps();
    ASSERT_TRUE(bCheckParsed);
    EXPECT_EQ(checkPersistor.CachedBootstrapCount(), std::uint32_t(2));
    EXPECT_EQ(checkPersistor.CachedBootstrapCount(Endpoints::TechnologyType::TCP), std::uint32_t(1));
    EXPECT_EQ(checkPersistor.CachedBootstrapCount(Endpoints::TechnologyType::Direct), std::uint32_t(1));

    std::filesystem::remove(filepath);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseGoodFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    auto const bParsed = persistor.FetchBootstraps();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedBootstrapCount(), std::uint32_t(1));
    EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerTechnology), std::uint32_t(1));

    persistor.ForEachCachedBootstrap(
        test::PeerTechnology,
        [] (std::string_view const& bootstrap) -> CallbackIteration
        {
            EXPECT_EQ(bootstrap, test::TcpBootstrapEntry);
            return CallbackIteration::Continue;
        }
    );
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMalformedFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/malformed/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    bool const bParsed = persistor.FetchBootstraps();
    EXPECT_FALSE(bParsed);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMissingPeersFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/missing/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    bool const bParsed = persistor.FetchBootstraps();
    std::uint32_t const count = persistor.CachedBootstrapCount(test::PeerTechnology);
    EXPECT_TRUE(bParsed);
    EXPECT_EQ(count, std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, PeerStateChangeTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/peers.json";
    CPeerPersistor persistor(filepath.c_str());

    // Check the initial state of the cached peers
    bool const bParsed = persistor.FetchBootstraps();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerTechnology), std::uint32_t(1));

    // Create a new peer and notify the persistor
    auto const spBryptPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    spBryptPeer->RegisterEndpoint(
        test::EndpointIdentifier, test::PeerTechnology, {}, test::NewBootstrapEntry);

    persistor.HandlePeerStateChange(
        spBryptPeer, test::EndpointIdentifier, test::PeerTechnology, ConnectionState::Connected);

    // Verify the new peer has been added to the current persistor
    EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerTechnology), std::uint32_t(2));
    
    bool bFoundConnectedBootstrap;
    persistor.ForEachCachedBootstrap(
        test::PeerTechnology,
        [&bFoundConnectedBootstrap] (std::string_view const& bootstrap) -> CallbackIteration
        {
            if (bootstrap == test::NewBootstrapEntry) {
                bFoundConnectedBootstrap = true;
                return CallbackIteration::Stop;
            }
            return CallbackIteration::Continue;
        }
    );
    EXPECT_TRUE(bFoundConnectedBootstrap);

    // Verify that a new persistor can read the updates
    {
        auto checkPersistor = std::make_unique<CPeerPersistor>(filepath.c_str());
        bool const bCheckParsed = checkPersistor->FetchBootstraps();
        ASSERT_TRUE(bCheckParsed);
        EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerTechnology), std::uint32_t(2));

        bool bFoundCheckBootstrap;
        persistor.ForEachCachedBootstrap(
            test::PeerTechnology,
            [&bFoundCheckBootstrap] (std::string_view const& bootstrap) -> CallbackIteration
            {
                if (bootstrap == test::NewBootstrapEntry) {
                    bFoundCheckBootstrap = true;
                    return CallbackIteration::Stop;
                }
                return CallbackIteration::Continue;
            }
        );

        EXPECT_TRUE(bFoundCheckBootstrap);
    }

    // Tell the persistor the new peer has been disconnected
    persistor.HandlePeerStateChange(
        spBryptPeer, test::EndpointIdentifier, test::PeerTechnology, ConnectionState::Disconnected);
    spBryptPeer->WithdrawEndpoint(test::EndpointIdentifier, test::PeerTechnology);

    persistor.FetchBootstraps(); // Force the persitor to re-query the persistor file
    EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerTechnology), std::uint32_t(1));

    // Verify the peer added from this test has been removed
    bool bFoundDisconnectedBootstrap = false;
    persistor.ForEachCachedBootstrap(
        test::PeerTechnology,
        [&bFoundDisconnectedBootstrap] (std::string_view const& bootstrap) -> CallbackIteration
        {
            if (bootstrap == test::NewBootstrapEntry) {
                bFoundDisconnectedBootstrap = true;
            }
            return CallbackIteration::Continue;
        }
    );
    EXPECT_FALSE(bFoundDisconnectedBootstrap);
}

//------------------------------------------------------------------------------------------------