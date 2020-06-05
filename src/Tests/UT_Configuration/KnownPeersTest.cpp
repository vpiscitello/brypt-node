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
    CPeerPersistor persistor(filepath.c_str());
    auto const spEndpointPeers = persistor.FetchPeers();
    ASSERT_TRUE(spEndpointPeers);
    EXPECT_EQ(spEndpointPeers->size(), std::size_t(1));

    auto const spTCPPeers = persistor.GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(spTCPPeers);
    EXPECT_EQ(spTCPPeers->size(), std::size_t(1));

    auto const itr = spTCPPeers->find(test::PeerId);
    ASSERT_NE(itr, spTCPPeers->end());

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
    CPeerPersistor persistor(filepath.c_str());
    auto const spEndpointPeers = persistor.FetchPeers();
    EXPECT_FALSE(spEndpointPeers);
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMissingPeersFileTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/missing/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    auto const spEndpointPeers = persistor.FetchPeers();
    EXPECT_TRUE(spEndpointPeers);

    auto const spTCPPeers = persistor.GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(spTCPPeers);
    EXPECT_EQ(spTCPPeers->size(), std::size_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, PeerStateChangeTest)
{
    std::filesystem::path const filepath = "./Tests/UT_Configuration/files/good/peers.json";
    CPeerPersistor persistor(filepath.c_str());
    auto const spEndpointPeers = persistor.FetchPeers();
    ASSERT_TRUE(spEndpointPeers);
    EXPECT_EQ(spEndpointPeers->size(), std::size_t(1));

    auto spTCPPeers = persistor.GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(spTCPPeers);
    EXPECT_EQ(spTCPPeers->size(), std::size_t(1));

    auto const existingItr = spTCPPeers->find(test::PeerId);
    ASSERT_NE(existingItr, spTCPPeers->end());

    auto const& [existingId, existingPeer] = *existingItr;
    EXPECT_EQ(existingId, test::PeerId);
    EXPECT_EQ(existingPeer.GetNodeId(), test::PeerId);
    EXPECT_EQ(existingPeer.GetEntry(), test::PeerEntry);
    EXPECT_TRUE(existingPeer.GetLocation().empty());
    EXPECT_EQ(existingPeer.GetTechnologyType(), test::PeerTechnology);

    auto const checkItr = spTCPPeers->find(test::NewPeerId);
    EXPECT_EQ(checkItr, spTCPPeers->end());

    CPeer peer(
        test::NewPeerId,
        test::PeerTechnology,
        test::NewPeerEntry);
    
    persistor.HandlePeerConnectionStateChange(peer, ConnectionState::Connected);

    spTCPPeers = persistor.GetCachedPeers(test::PeerTechnology);
    auto const connectedItr = spTCPPeers->find(test::NewPeerId);
    ASSERT_NE(connectedItr, spTCPPeers->end());
    auto const& [connectedId, connectedPeer] = *connectedItr;

    auto checkPersistor = std::make_unique<CPeerPersistor>(filepath.c_str());
    checkPersistor->FetchPeers();

    auto spCheckTCPPeers = checkPersistor->GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(spCheckTCPPeers);

    auto const checkConnectedItr = spCheckTCPPeers->find(connectedPeer.GetNodeId());
    ASSERT_NE(checkConnectedItr, spCheckTCPPeers->end());

    auto const& [checkConnectedId, checkConnectedPeer] = *checkConnectedItr;
    EXPECT_EQ(checkConnectedId, connectedPeer.GetNodeId());
    EXPECT_EQ(checkConnectedPeer.GetNodeId(), connectedPeer.GetNodeId());
    EXPECT_EQ(checkConnectedPeer.GetEntry(), connectedPeer.GetEntry());
    EXPECT_EQ(checkConnectedPeer.GetLocation(), connectedPeer.GetLocation());
    EXPECT_EQ(checkConnectedPeer.GetTechnologyType(), connectedPeer.GetTechnologyType());

    checkPersistor.reset();

    persistor.HandlePeerConnectionStateChange(peer, ConnectionState::Disconnected);
    
    checkPersistor = std::make_unique<CPeerPersistor>(filepath.c_str());
    checkPersistor->FetchPeers();

    spCheckTCPPeers = checkPersistor->GetCachedPeers(test::PeerTechnology);
    ASSERT_TRUE(spCheckTCPPeers);

    auto const checkDisconnectedItr = spCheckTCPPeers->find(connectedPeer.GetNodeId());
    ASSERT_EQ(checkDisconnectedItr, spCheckTCPPeers->end());
}

//------------------------------------------------------------------------------------------------