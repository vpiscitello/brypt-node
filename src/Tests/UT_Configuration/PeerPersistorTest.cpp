//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "Components/Configuration/Configuration.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Utilities/NodeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

Configuration::EndpointOptions GenerateTcpOptions();
Configuration::EndpointOptions GenerateLoRaOptions();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

constexpr Network::Endpoint::Identifier EndpointIdentifier = 1;
constexpr Network::Protocol PeerProtocol = Network::Protocol::TCP;

constexpr std::string_view TcpBootstrapEntry = "tcp://127.0.0.1:35216";
constexpr std::string_view LoraBootstrapEntry = "lora://915:71";

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, GeneratePeersFilepathTest)
{
    auto const filepath = Configuration::GetDefaultPeersFilepath();
    EXPECT_TRUE(filepath.has_parent_path());
    EXPECT_TRUE(filepath.is_absolute());
    auto const found = filepath.string().find(Configuration::DefaultBryptFolder);
    EXPECT_NE(found, std::string::npos);
    EXPECT_EQ(filepath.filename(), Configuration::DefaultKnownPeersFilename);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, DefualtBootstrapTest)
{
    std::filesystem::path const filepath = "files/good/default-peers.json";

    Configuration::EndpointConfigurations configurations;
    configurations.emplace_back(local::GenerateTcpOptions());
    configurations.emplace_back(local::GenerateLoRaOptions());
    for (auto& options : configurations) { ASSERT_TRUE(options.Initialize()); }

    PeerPersistor persistor(filepath.c_str(), configurations);
    auto const bParsed = persistor.FetchBootstraps();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedBootstrapCount(), std::size_t(2));
    EXPECT_EQ(persistor.CachedBootstrapCount(Network::Protocol::TCP), std::size_t(1));
    EXPECT_EQ(persistor.CachedBootstrapCount(Network::Protocol::LoRa), std::size_t(1));

    PeerPersistor checkPersistor(filepath.c_str(), configurations);
    auto const bCheckParsed = checkPersistor.FetchBootstraps();
    ASSERT_TRUE(bCheckParsed);
    EXPECT_EQ(checkPersistor.CachedBootstrapCount(), std::size_t(2));
    EXPECT_EQ(checkPersistor.CachedBootstrapCount(Network::Protocol::TCP), std::size_t(1));
    EXPECT_EQ(checkPersistor.CachedBootstrapCount(Network::Protocol::LoRa), std::size_t(1));

    std::filesystem::remove(filepath);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseGoodFileTest)
{
    std::filesystem::path const filepath = "files/good/peers.json";
    PeerPersistor persistor(filepath.c_str());
    auto const bParsed = persistor.FetchBootstraps();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedBootstrapCount(), std::size_t(1));
    EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerProtocol), std::size_t(1));

    persistor.ForEachCachedBootstrap(
        test::PeerProtocol,
        [] (Network::RemoteAddress const& bootstrap) -> CallbackIteration
        {
            EXPECT_EQ(bootstrap.GetUri(), test::TcpBootstrapEntry);
            return CallbackIteration::Continue;
        }
    );
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMalformedFileTest)
{
    std::filesystem::path const filepath = "files/malformed/peers.json";
    PeerPersistor persistor(filepath.c_str());
    bool const bParsed = persistor.FetchBootstraps();
    EXPECT_FALSE(bParsed);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, ParseMissingPeersFileTest)
{
    std::filesystem::path const filepath = "files/missing/peers.json";
    PeerPersistor persistor(filepath.c_str());
    bool const bParsed = persistor.FetchBootstraps();
    std::size_t const count = persistor.CachedBootstrapCount(test::PeerProtocol);
    EXPECT_TRUE(bParsed);
    EXPECT_EQ(count, std::size_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerPersistorSuite, PeerStateChangeTest)
{
    std::filesystem::path const filepath = "files/good/peers.json";
    PeerPersistor persistor(filepath.c_str());

    // Check the initial state of the cached peers
    bool const bParsed = persistor.FetchBootstraps();
    ASSERT_TRUE(bParsed);
    EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerProtocol), std::size_t(1));

    Network::RemoteAddress const address(Network::Protocol::TCP, "127.0.0.1:35220", true);

    // Create a new peer and notify the persistor
    auto const spPeerProxy = std::make_shared<Peer::Proxy>(Node::Identifier{ Node::GenerateIdentifier() });
    spPeerProxy->RegisterEndpoint(test::EndpointIdentifier, test::PeerProtocol, address, {});

    persistor.HandlePeerStateChange(
        spPeerProxy, test::EndpointIdentifier, test::PeerProtocol, ConnectionState::Connected);

    // Verify the new peer has been added to the current persistor
    EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerProtocol), std::size_t(2));
    
    bool bFoundConnectedBootstrap;
    persistor.ForEachCachedBootstrap(
        test::PeerProtocol,
        [&bFoundConnectedBootstrap, &address] (Network::RemoteAddress const& bootstrap) 
            -> CallbackIteration
        {
            if (bootstrap == address) {
                bFoundConnectedBootstrap = true;
                return CallbackIteration::Stop;
            }
            return CallbackIteration::Continue;
        }
    );
    EXPECT_TRUE(bFoundConnectedBootstrap);

    // Verify that a new persistor can read the updates
    {
        auto checkPersistor = std::make_unique<PeerPersistor>(filepath.c_str());
        bool const bCheckParsed = checkPersistor->FetchBootstraps();
        ASSERT_TRUE(bCheckParsed);
        EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerProtocol), std::size_t(2));

        bool bFoundCheckBootstrap;
        persistor.ForEachCachedBootstrap(
            test::PeerProtocol,
            [&bFoundCheckBootstrap, &address] (Network::RemoteAddress const& bootstrap)
                -> CallbackIteration
            {
                if (bootstrap == address) {
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
        spPeerProxy, test::EndpointIdentifier, test::PeerProtocol, ConnectionState::Disconnected);
    spPeerProxy->WithdrawEndpoint(test::EndpointIdentifier, test::PeerProtocol);

    persistor.FetchBootstraps(); // Force the persitor to re-query the persistor file
    EXPECT_EQ(persistor.CachedBootstrapCount(test::PeerProtocol), std::size_t(1));

    // Verify the peer added from this test has been removed
    bool bFoundDisconnectedBootstrap = false;
    persistor.ForEachCachedBootstrap(
        test::PeerProtocol,
        [&bFoundDisconnectedBootstrap, &address] (Network::RemoteAddress const& bootstrap)
            -> CallbackIteration
        {
            if (bootstrap == address) { bFoundDisconnectedBootstrap = true; }
            return CallbackIteration::Continue;
        }
    );
    EXPECT_FALSE(bFoundDisconnectedBootstrap);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::EndpointOptions local::GenerateTcpOptions()
{
    Configuration::EndpointOptions options;
    options.type = Network::Protocol::TCP;
    options.interface = "lo";
    options.binding = test::TcpBootstrapEntry;
    options.bootstrap = test::TcpBootstrapEntry;
    return options;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::EndpointOptions local::GenerateLoRaOptions()
{
    Configuration::EndpointOptions options;
    options.type = Network::Protocol::LoRa;
    options.interface = "lo";
    options.binding = test::LoraBootstrapEntry;
    options.bootstrap = test::LoraBootstrapEntry;
    return options;
}

//----------------------------------------------------------------------------------------------------------------------
