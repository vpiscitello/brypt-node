//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../../Components/Endpoints/Peer.hpp"
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

constexpr std::string_view TcpBootstrapEntry = "127.0.0.1:35216";
constexpr std::string_view DirectBootstrapEntry = "127.0.0.1:35217";

BryptIdentifier::CContainer const KnownPeerId("bry0:37GDnYQnHhqkVfV6UGyXudsZTU3q");
constexpr Endpoints::TechnologyType PeerTechnology = Endpoints::TechnologyType::TCP;
constexpr std::string_view PeerEntry = "127.0.0.1:35216";

BryptIdentifier::CContainer const NewPeerId(BryptIdentifier::Generate());
constexpr std::string_view NewPeerEntry = "127.0.0.1:35217";

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
    auto const bParsed = persistor.FetchPeers();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedEndpointsCount(), std::uint32_t(2));
    EXPECT_EQ(persistor.CachedPeersCount(), std::uint32_t(2));
    EXPECT_EQ(persistor.CachedPeersCount(Endpoints::TechnologyType::TCP), std::uint32_t(1));
    EXPECT_EQ(persistor.CachedPeersCount(Endpoints::TechnologyType::Direct), std::uint32_t(1));

    CPeerPersistor checkPersistor(filepath.c_str(), configurations);
    auto const bCheckParsed = checkPersistor.FetchPeers();
    ASSERT_TRUE(bCheckParsed);
    EXPECT_EQ(checkPersistor.CachedEndpointsCount(), std::uint32_t(2));
    EXPECT_EQ(checkPersistor.CachedPeersCount(), std::uint32_t(2));
    EXPECT_EQ(checkPersistor.CachedPeersCount(Endpoints::TechnologyType::TCP), std::uint32_t(1));
    EXPECT_EQ(checkPersistor.CachedPeersCount(Endpoints::TechnologyType::Direct), std::uint32_t(1));

    std::filesystem::remove(filepath);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseGoodFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    auto const bParsed = persistor.FetchPeers();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedEndpointsCount(), std::uint32_t(1));
    EXPECT_EQ(persistor.CachedPeersCount(), std::uint32_t(1));
    EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::uint32_t(1));

    std::uint32_t iterations = 0;
    CPeer foundPeer;
    ASSERT_EQ(foundPeer.GetIdentifier(), BryptIdentifier::CContainer());
    persistor.ForEachCachedPeer(
        test::PeerTechnology,
        [&iterations, &foundPeer] (CPeer const& peer) -> CallbackIteration {
            if (peer.GetIdentifier() == test::KnownPeerId) {
                foundPeer = peer;
            }
            ++iterations;
            return CallbackIteration::Continue;
        }
    );
    ASSERT_EQ(iterations, std::uint32_t(1));
    ASSERT_EQ(foundPeer.GetIdentifier(), test::KnownPeerId);
    EXPECT_EQ(foundPeer.GetEntry(), test::PeerEntry);
    EXPECT_TRUE(foundPeer.GetLocation().empty());
    EXPECT_EQ(foundPeer.GetTechnologyType(), test::PeerTechnology);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMalformedFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/malformed/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    bool const bParsed = persistor.FetchPeers();
    EXPECT_FALSE(bParsed);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMissingPeersFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/missing/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    bool const bParsed = persistor.FetchPeers();
    std::uint32_t const count = persistor.CachedPeersCount(test::PeerTechnology);
    EXPECT_TRUE(bParsed);
    EXPECT_EQ(count, std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, PeerStateChangeTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/peers.json";
    CPeerPersistor persistor(filepath.c_str());

    // Check the initial state of the cached peers
    bool const bParsed = persistor.FetchPeers();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedEndpointsCount(), std::uint32_t(1));
    EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::uint32_t(1));

    CPeer initialPeer;
    persistor.ForEachCachedPeer(
        test::PeerTechnology,
        [&initialPeer] (CPeer const& peer) -> CallbackIteration {
            if (peer.GetIdentifier() == test::KnownPeerId) {
                initialPeer = peer;
                return CallbackIteration::Stop;
            }
            return CallbackIteration::Continue;
        }
    );
    ASSERT_EQ(initialPeer.GetIdentifier(), test::KnownPeerId);

    // Create a new peer and notify the persistor
    CPeer newPeer(test::NewPeerId, test::PeerTechnology, test::NewPeerEntry);
    persistor.HandlePeerConnectionStateChange(newPeer, ConnectionState::Connected);

    // Verify the new peer has been added to the current persistor
    EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::uint32_t(2));

    CPeer newConnectedPeer;
    persistor.ForEachCachedPeer(
        test::PeerTechnology,
        [&newConnectedPeer] (CPeer const& peer) -> CallbackIteration {
            if (peer.GetIdentifier() == test::NewPeerId) {
                newConnectedPeer = peer;
                return CallbackIteration::Stop;
            }
            return CallbackIteration::Continue;
        }
    );
    ASSERT_EQ(newConnectedPeer.GetIdentifier(), test::NewPeerId);

    // Verify that a new persistor can read the updates
    {
        auto checkPersistor = std::make_unique<CPeerPersistor>(filepath.c_str());
        bool const bCheckParsed = checkPersistor->FetchPeers();
        ASSERT_TRUE(bCheckParsed);
        EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::uint32_t(2));

        CPeer checkConnectedPeer;
        persistor.ForEachCachedPeer(
            test::PeerTechnology,
            [&checkConnectedPeer] (CPeer const& peer) -> CallbackIteration {
                if (peer.GetIdentifier() == test::NewPeerId) {
                    checkConnectedPeer = peer;
                    return CallbackIteration::Stop;
                }
                return CallbackIteration::Continue;
            }
        );

        // Verify the values that were read from the new persistor match
        EXPECT_EQ(checkConnectedPeer.GetIdentifier(), newConnectedPeer.GetIdentifier());
        EXPECT_EQ(checkConnectedPeer.GetEntry(), newConnectedPeer.GetEntry());
        EXPECT_EQ(checkConnectedPeer.GetLocation(), newConnectedPeer.GetLocation());
        EXPECT_EQ(checkConnectedPeer.GetTechnologyType(), newConnectedPeer.GetTechnologyType());
    }

    // Tell the persistor the new peer has been disconnected
    persistor.HandlePeerConnectionStateChange(newPeer, ConnectionState::Disconnected);
    
    persistor.FetchPeers(); // Force the persitor to re-query the persistor file
    EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::uint32_t(1));

    // Verify the peer added from this test has been removed
    bool bFoundNewPeer = false;
    persistor.ForEachCachedPeer(
        test::PeerTechnology,
        [&bFoundNewPeer] (CPeer const& peer) -> CallbackIteration {
            if (peer.GetIdentifier() == test::NewPeerId) {
                bFoundNewPeer = true;
            }
            return CallbackIteration::Continue;
        }
    );
    ASSERT_FALSE(bFoundNewPeer);
}

//------------------------------------------------------------------------------------------------