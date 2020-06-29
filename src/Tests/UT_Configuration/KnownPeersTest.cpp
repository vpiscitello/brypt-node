//------------------------------------------------------------------------------------------------
#include "../../Components/Endpoints/Peer.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Configuration/Configuration.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "../../Utilities/ReservedIdentifiers.hpp"
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

constexpr Endpoints::TechnologyType PeerTechnology = Endpoints::TechnologyType::TCP;
constexpr NodeUtils::NodeIdType PeerId = 305419896;
constexpr std::string_view PeerEntry = "127.0.0.1:35216";

constexpr NodeUtils::NodeIdType NewPeerId = 0xAABBCCDD;
constexpr std::string_view NewPeerEntry = "127.0.0.1:35217";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(ConfigurationManagerSuite, GeneratePeersFilepathTest)
{
    auto const filepath = Configuration::GetDefaultPeersFilepath();
    EXPECT_TRUE(filepath.has_parent_path());
    EXPECT_TRUE(filepath.is_absolute());
    auto const found = filepath.string().find(Configuration::DefaultBryptFolder);
    EXPECT_NE(found, std::string::npos);
    EXPECT_EQ(filepath.filename(), Configuration::DefaultKnownPeersFilename);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseGoodFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    auto const bParsed = persistor.FetchPeers();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedEndpointsCount(), std::size_t(1));
    EXPECT_EQ(persistor.CachedPeersCount(), std::size_t(1));
    EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::size_t(1));

    std::uint32_t iterations = 0;
    CPeer foundPeer;
    ASSERT_EQ(foundPeer.GetNodeId(), static_cast<NodeUtils::NodeIdType>(ReservedIdentifiers::Invalid));
    persistor.ForEachCachedPeer(
        test::PeerTechnology,
        [&iterations, &foundPeer] (CPeer const& peer) -> CallbackIteration {
            if (peer.GetNodeId() == test::PeerId) {
                foundPeer = peer;
            }
            ++iterations;
            return CallbackIteration::Continue;
        }
    );
    ASSERT_EQ(iterations, std::uint32_t(1));
    ASSERT_EQ(foundPeer.GetNodeId(), test::PeerId);
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
    EXPECT_EQ(count, std::uint32_t(1));
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, PeerStateChangeTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/peers.json";
    CPeerPersistor persistor(filepath.c_str());

    // Check the initial state of the cached peers
    bool const bParsed = persistor.FetchPeers();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedEndpointsCount(), std::size_t(1));
    EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::size_t(1));

    CPeer initialPeer;
    persistor.ForEachCachedPeer(
        test::PeerTechnology,
        [&initialPeer] (CPeer const& peer) -> CallbackIteration {
            if (peer.GetNodeId() == test::PeerId) {
                initialPeer = peer;
                return CallbackIteration::Stop;
            }
            return CallbackIteration::Continue;
        }
    );
    ASSERT_EQ(initialPeer.GetNodeId(), test::PeerId);

    // Create a new peer and notify the persistor
    CPeer newPeer(test::NewPeerId, test::PeerTechnology, test::NewPeerEntry);
    persistor.HandlePeerConnectionStateChange(newPeer, ConnectionState::Connected);

    // Verify the new peer has been added to the current persistor
    EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::size_t(2));

    CPeer newConnectedPeer;
    persistor.ForEachCachedPeer(
        test::PeerTechnology,
        [&newConnectedPeer] (CPeer const& peer) -> CallbackIteration {
            if (peer.GetNodeId() == test::NewPeerId) {
                newConnectedPeer = peer;
                return CallbackIteration::Stop;
            }
            return CallbackIteration::Continue;
        }
    );
    ASSERT_EQ(newConnectedPeer.GetNodeId(), test::NewPeerId);

    // Verify that a new persistor can read the updates
    {
        auto checkPersistor = std::make_unique<CPeerPersistor>(filepath.c_str());
        bool const bCheckParsed = checkPersistor->FetchPeers();
        ASSERT_TRUE(bCheckParsed);
        EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::size_t(2));

        CPeer checkConnectedPeer;
        persistor.ForEachCachedPeer(
            test::PeerTechnology,
            [&checkConnectedPeer] (CPeer const& peer) -> CallbackIteration {
                if (peer.GetNodeId() == test::NewPeerId) {
                    checkConnectedPeer = peer;
                    return CallbackIteration::Stop;
                }
                return CallbackIteration::Continue;
            }
        );

        // Verify the values that were read from the new persistor match
        EXPECT_EQ(checkConnectedPeer.GetNodeId(), newConnectedPeer.GetNodeId());
        EXPECT_EQ(checkConnectedPeer.GetEntry(), newConnectedPeer.GetEntry());
        EXPECT_EQ(checkConnectedPeer.GetLocation(), newConnectedPeer.GetLocation());
        EXPECT_EQ(checkConnectedPeer.GetTechnologyType(), newConnectedPeer.GetTechnologyType());
    }

    // Tell the persistor the new peer has been disconnected
    persistor.HandlePeerConnectionStateChange(newPeer, ConnectionState::Disconnected);
    
    persistor.FetchPeers(); // Force the persitor to re-query the persistor file
    EXPECT_EQ(persistor.CachedPeersCount(test::PeerTechnology), std::size_t(1));

    // Verify the peer added from this test has been removed
    bool bFoundNewPeer = false;
    persistor.ForEachCachedPeer(
        test::PeerTechnology,
        [&bFoundNewPeer] (CPeer const& peer) -> CallbackIteration {
            if (peer.GetNodeId() == test::NewPeerId) {
                bFoundNewPeer = true;
            }
            return CallbackIteration::Continue;
        }
    );
    ASSERT_FALSE(bFoundNewPeer);
}

//------------------------------------------------------------------------------------------------