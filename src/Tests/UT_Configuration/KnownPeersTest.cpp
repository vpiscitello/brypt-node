//------------------------------------------------------------------------------------------------
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
    Configuration::CPeerPersistor persistor(filepath.c_str());
    auto const optEndpointPeers = persistor.FetchPeers();
    ASSERT_TRUE(optEndpointPeers);
    EXPECT_EQ(optEndpointPeers->size(), std::size_t(1));

    auto const optTCPPeers = persistor.GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(optTCPPeers);
    EXPECT_EQ(optTCPPeers->size(), std::size_t(1));

    auto const itr = optTCPPeers->find(test::PeerId);
    ASSERT_NE(itr, optTCPPeers->end());

    auto const& [id, peer] = *itr;
    EXPECT_EQ(id, test::PeerId);
    EXPECT_EQ(peer.GetNodeId(), test::PeerId);
    EXPECT_EQ(peer.GetEntry(), test::PeerEntry);
    EXPECT_TRUE(peer.GetLocation().empty());
    EXPECT_EQ(peer.GetTechnologyType(), test::PeerTechnology);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMalformedFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/malformed/peers.json";
    Configuration::CPeerPersistor persistor(filepath.c_str());
    auto const optEndpointPeers = persistor.FetchPeers();
    EXPECT_FALSE(optEndpointPeers);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMissingPeersFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/missing/peers.json";
    Configuration::CPeerPersistor persistor(filepath.c_str());
    auto const optEndpointPeers = persistor.FetchPeers();
    EXPECT_TRUE(optEndpointPeers);

    auto const optTCPPeers = persistor.GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(optTCPPeers);
    EXPECT_EQ(optTCPPeers->size(), std::size_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, PeerStateChangeTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/peers.json";
    Configuration::CPeerPersistor persistor(filepath.c_str());
    auto const optEndpointPeers = persistor.FetchPeers();
    ASSERT_TRUE(optEndpointPeers);
    EXPECT_EQ(optEndpointPeers->size(), std::size_t(1));

    auto optTCPPeers = persistor.GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(optTCPPeers);
    EXPECT_EQ(optTCPPeers->size(), std::size_t(1));

    auto const existingItr = optTCPPeers->find(test::PeerId);
    ASSERT_NE(existingItr, optTCPPeers->end());

    auto const& [existingId, existingPeer] = *existingItr;
    EXPECT_EQ(existingId, test::PeerId);
    EXPECT_EQ(existingPeer.GetNodeId(), test::PeerId);
    EXPECT_EQ(existingPeer.GetEntry(), test::PeerEntry);
    EXPECT_TRUE(existingPeer.GetLocation().empty());
    EXPECT_EQ(existingPeer.GetTechnologyType(), test::PeerTechnology);

    auto const checkItr = optTCPPeers->find(test::NewPeerId);
    EXPECT_EQ(checkItr, optTCPPeers->end());

    CPeer peer(
        test::NewPeerId,
        test::PeerTechnology,
        test::NewPeerEntry);
    
    persistor.HandlePeerConnectionStateChange(peer, ConnectionState::Connected);

    optTCPPeers = persistor.GetCachedPeers(test::PeerTechnology);
    auto const connectedItr = optTCPPeers->find(test::NewPeerId);
    ASSERT_NE(connectedItr, optTCPPeers->end());
    auto const& [connectedId, connectedPeer] = *connectedItr;

    auto checkPersistor = std::make_unique<Configuration::CPeerPersistor>(filepath.c_str());
    checkPersistor->FetchPeers();

    auto optCheckTCPPeers = checkPersistor->GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(optCheckTCPPeers);

    auto const checkConnectedItr = optCheckTCPPeers->find(connectedPeer.GetNodeId());
    ASSERT_NE(checkConnectedItr, optCheckTCPPeers->end());

    auto const& [checkConnectedId, checkConnectedPeer] = *checkConnectedItr;
    EXPECT_EQ(checkConnectedId, connectedPeer.GetNodeId());
    EXPECT_EQ(checkConnectedPeer.GetNodeId(), connectedPeer.GetNodeId());
    EXPECT_EQ(checkConnectedPeer.GetEntry(), connectedPeer.GetEntry());
    EXPECT_EQ(checkConnectedPeer.GetLocation(), connectedPeer.GetLocation());
    EXPECT_EQ(checkConnectedPeer.GetTechnologyType(), connectedPeer.GetTechnologyType());

    checkPersistor.reset();

    persistor.HandlePeerConnectionStateChange(peer, ConnectionState::Disconnected);
    
    checkPersistor = std::make_unique<Configuration::CPeerPersistor>(filepath.c_str());
    checkPersistor->FetchPeers();

    optCheckTCPPeers = checkPersistor->GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(optCheckTCPPeers);

    auto const checkDisconnectedItr = optCheckTCPPeers->find(connectedPeer.GetNodeId());
    ASSERT_EQ(checkDisconnectedItr, optCheckTCPPeers->end());
}

//------------------------------------------------------------------------------------------------