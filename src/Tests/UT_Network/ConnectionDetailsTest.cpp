
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Utilities/NodeUtils.hpp"
#include "Utilities/TimeUtils.hpp"
#include "Components/Network/ConnectionTracker.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <chrono>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
Node::Identifier const ServerIdentifier(Node::GenerateIdentifier());

constexpr std::string_view ProtocolName = "TCP";
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35222";
constexpr std::string_view ClientBinding = "*:35223";
constexpr std::string_view ServerEntry = "127.0.0.1:35222";
constexpr std::string_view ClientEntry = "127.0.0.1:35223";

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------

TEST(ConnectionDetailsSuite, IdentifierTranslateTest)
{
    ConnectionTracker<std::string> tracker;
    
    std::string const connection = "1";
    tracker.TrackConnection(connection);

    auto const spFirstIdentifier = tracker.Translate(connection);
    EXPECT_FALSE(spFirstIdentifier);

    auto const spPeerProxy = Peer::Proxy::CreateInstance(Node::Identifier{ Node::GenerateIdentifier() });
    auto const spNodeIdentifier = spPeerProxy->GetIdentifier();

    auto const firstConnectionIdentifier = tracker.Translate(*spNodeIdentifier);
    EXPECT_TRUE(firstConnectionIdentifier.empty());

    tracker.UpdateOneConnection(connection,
        [] ([[maybe_unused]] auto& details) { ASSERT_FALSE(true); },
        [this, &spPeerProxy] ([[maybe_unused]] Network::RemoteAddress const& address) -> ConnectionDetails<>
        {
            ConnectionDetails<> details(spPeerProxy);
            details.SetConnectionState(ConnectionState::Unknown);
            return details;
        }
    );
    
    auto const spSecondIdentifier = tracker.Translate(connection);
    auto const secondConnectionIdentifier = tracker.Translate(*spNodeIdentifier);
    EXPECT_EQ(spNodeIdentifier, spSecondIdentifier);
    EXPECT_EQ(*spNodeIdentifier, *spSecondIdentifier);
    EXPECT_EQ(connection, secondConnectionIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, SingleConnectionTest)
{
    ConnectionTracker<std::string> tracker;
    
    std::string const clientConnectionId = "1";
    auto const spClientPeer = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    ConnectionDetails<> details(spClientPeer);
    details.SetConnectionState(ConnectionState::Unknown);

    tracker.TrackConnection(clientConnectionId, details);

    auto const connectionIdentifier = tracker.Translate(test::ClientIdentifier);
    EXPECT_EQ(connectionIdentifier, clientConnectionId);

    auto const spNodeIdentifier = tracker.Translate(clientConnectionId);
    ASSERT_TRUE(spNodeIdentifier);
    EXPECT_EQ(*spNodeIdentifier, test::ClientIdentifier);

    bool const bFirstNodeReadFound = tracker.ReadOneConnection(clientConnectionId,
        [](auto const& details)
        {
            EXPECT_EQ(details.GetConnectionState(), ConnectionState::Unknown);
        }
    );
    EXPECT_TRUE(bFirstNodeReadFound);

    bool const bFirstNodeUpdateFound = tracker.UpdateOneConnection(clientConnectionId,
        [](auto& details)
        {
            details.SetConnectionState(ConnectionState::Connected);
        }
    );
    EXPECT_TRUE(bFirstNodeUpdateFound);

    bool const bSecondNodeReadFound = tracker.ReadOneConnection(clientConnectionId,
        [](auto const& details)
        {
            EXPECT_EQ(details.GetConnectionState(), ConnectionState::Connected);
        }
    );
    EXPECT_TRUE(bSecondNodeReadFound);

    tracker.UntrackConnection(clientConnectionId);

    bool const bThirdNodeReadFound = tracker.ReadOneConnection(clientConnectionId,
        []([[maybe_unused]] auto const& details){}
    );
    EXPECT_FALSE(bThirdNodeReadFound);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, MultipleConnectionsTest)
{
    ConnectionTracker<std::string> tracker;
    
    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = Peer::Proxy::CreateInstance(
        Node::Identifier{ Node::GenerateIdentifier() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetIdentifier();
    ConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetConnectionState(ConnectionState::Unknown);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = Peer::Proxy::CreateInstance(
        Node::Identifier{ Node::GenerateIdentifier() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetIdentifier();
    ConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetConnectionState(ConnectionState::Unknown);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = Peer::Proxy::CreateInstance(
        Node::Identifier{ Node::GenerateIdentifier() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetIdentifier();
    ConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetConnectionState(ConnectionState::Unknown);

    tracker.TrackConnection(firstConnectionIdentifier, firstConnectionDetails);
    tracker.TrackConnection(secondConnectionIdentifier, secondConnectionDetails);
    tracker.TrackConnection(thirdConnectionIdentifier, thirdConnectionDetails);

    auto const connectionIdentifier = tracker.Translate(*spSecondNodeIdentifier);
    EXPECT_EQ(connectionIdentifier, secondConnectionIdentifier);

    auto const spNodeIdentifier = tracker.Translate(firstConnectionIdentifier);
    ASSERT_TRUE(spNodeIdentifier);
    EXPECT_EQ(*spNodeIdentifier, *spFirstNodeIdentifier);

    bool const bFirstNodeReadFound = tracker.ReadOneConnection(secondConnectionIdentifier,
        [](auto const& details)
        {
            EXPECT_EQ(details.GetConnectionState(), ConnectionState::Unknown);
        }
    );
    EXPECT_TRUE(bFirstNodeReadFound);

    bool const bFirstNodeUpdateFound = tracker.UpdateOneConnection(secondConnectionIdentifier,
        [](auto& details)
        {
            details.SetConnectionState(ConnectionState::Disconnected);
        }
    );
    EXPECT_TRUE(bFirstNodeUpdateFound);

    bool const bSecondNodeReadFound = tracker.ReadOneConnection(secondConnectionIdentifier,
        [](auto const& details)
        {
            EXPECT_EQ(details.GetConnectionState(), ConnectionState::Disconnected);
        }
    );
    EXPECT_TRUE(bSecondNodeReadFound);

    std::uint32_t updateCounter = 0;
    tracker.UpdateEachConnection(
        [&updateCounter]([[maybe_unused]] std::string const& id, auto& optDetails) -> CallbackIteration
        {
            EXPECT_TRUE(optDetails);
            if (!optDetails) {
                return CallbackIteration::Stop;
            }

            optDetails->SetConnectionState(ConnectionState::Connected);
            ++updateCounter;
            return CallbackIteration::Continue;
        }
    );
    EXPECT_EQ(updateCounter, std::uint32_t(3));

    tracker.UntrackConnection(firstConnectionIdentifier);

    std::uint32_t readCounter = 0;
    tracker.ReadEachConnection(
        [&readCounter]([[maybe_unused]] std::string const& id, auto const& optDetails) -> CallbackIteration
        {
            EXPECT_TRUE(optDetails);
            if (!optDetails) {
                return CallbackIteration::Stop;
            }

            EXPECT_EQ(optDetails->GetConnectionState(), ConnectionState::Connected);
            ++readCounter;
            return CallbackIteration::Continue;
        }
    );
    EXPECT_EQ(readCounter, std::uint32_t(2));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, ConnectionStateFilterTest)
{
    using namespace std::chrono_literals;
    ConnectionTracker<std::string> tracker;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();

    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = Peer::Proxy::CreateInstance(Node::Identifier{ Node::GenerateIdentifier() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetIdentifier();
    ConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetConnectionState(ConnectionState::Disconnected);
    firstConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = Peer::Proxy::CreateInstance(Node::Identifier{ Node::GenerateIdentifier() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetIdentifier();
    ConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetConnectionState(ConnectionState::Resolving);
    secondConnectionDetails.SetUpdatedTimepoint(timepoint - 10min);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = Peer::Proxy::CreateInstance(Node::Identifier{ Node::GenerateIdentifier() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetIdentifier();
    ConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetConnectionState(ConnectionState::Connected);
    thirdConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const fourthConnectionIdentifier = "4";

    tracker.TrackConnection(firstConnectionIdentifier, firstConnectionDetails);
    tracker.TrackConnection(secondConnectionIdentifier, secondConnectionDetails);
    tracker.TrackConnection(thirdConnectionIdentifier, thirdConnectionDetails);
    tracker.TrackConnection(fourthConnectionIdentifier);

    std::vector<std::string> readFoundIdentifiers;
    tracker.ReadEachConnection(
        [&readFoundIdentifiers](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            readFoundIdentifiers.push_back(id);
            return CallbackIteration::Continue;
        },
        ConnectionStateFilter::Connected
    );

    EXPECT_EQ(readFoundIdentifiers.size(), std::size_t(1));
    auto const foundThirdIdentifierInRead = std::find(
        readFoundIdentifiers.begin(), readFoundIdentifiers.end(), thirdConnectionIdentifier);
    EXPECT_NE(foundThirdIdentifierInRead, readFoundIdentifiers.end());

    std::vector<std::string> updateFoundIdentifiers;
    tracker.UpdateEachConnection(
        [&updateFoundIdentifiers](auto const& id, [[maybe_unused]] auto& optDetails) -> CallbackIteration
        {
            updateFoundIdentifiers.push_back(id);
            return CallbackIteration::Continue;
        },
        ConnectionStateFilter::Disconnected | ConnectionStateFilter::Resolving
    );

    EXPECT_EQ(updateFoundIdentifiers.size(), std::size_t(2));
    auto const foundFirstIdentifierInUpdate = std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), firstConnectionIdentifier);
    auto const foundSecondIdentifierInUpdate = std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), secondConnectionIdentifier);
    EXPECT_NE(foundFirstIdentifierInUpdate, updateFoundIdentifiers.end());
    EXPECT_NE(foundSecondIdentifierInUpdate, updateFoundIdentifiers.end());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, PromotionFilterTest)
{
    using namespace std::chrono_literals;
    ConnectionTracker<std::string> tracker;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = Peer::Proxy::CreateInstance(Node::Identifier{ Node::GenerateIdentifier() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetIdentifier();
    ConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetConnectionState(ConnectionState::Disconnected);
    firstConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = Peer::Proxy::CreateInstance(Node::Identifier{ Node::GenerateIdentifier() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetIdentifier();
    ConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetConnectionState(ConnectionState::Resolving);
    secondConnectionDetails.SetUpdatedTimepoint(timepoint - 10min);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = Peer::Proxy::CreateInstance(Node::Identifier{ Node::GenerateIdentifier() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetIdentifier();
    ConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetConnectionState(ConnectionState::Connected);
    thirdConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const fourthConnectionIdentifier = "4";

    tracker.TrackConnection(firstConnectionIdentifier, firstConnectionDetails);
    tracker.TrackConnection(secondConnectionIdentifier, secondConnectionDetails);
    tracker.TrackConnection(thirdConnectionIdentifier, thirdConnectionDetails);
    tracker.TrackConnection(fourthConnectionIdentifier);

    std::vector<std::string> readFoundIdentifiers;
    tracker.ReadEachConnection(
        [&readFoundIdentifiers](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            readFoundIdentifiers.push_back(id);
            return CallbackIteration::Continue;
        },
        PromotionStateFilter::Unpromoted
    );

    EXPECT_EQ(readFoundIdentifiers.size(), std::size_t(1));
    auto const foundFourthIdentifierInRead = std::find(
        readFoundIdentifiers.begin(), readFoundIdentifiers.end(), fourthConnectionIdentifier);
    EXPECT_NE(foundFourthIdentifierInRead, readFoundIdentifiers.end());

    std::vector<std::string> updateFoundIdentifiers;
    tracker.ReadEachConnection(
        [&updateFoundIdentifiers](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            updateFoundIdentifiers.push_back(id);
            return CallbackIteration::Continue;
        },
        PromotionStateFilter::Promoted
    );
    EXPECT_EQ(updateFoundIdentifiers.size(), std::size_t(3));
    auto const foundFourthIdentfierInUpdate = std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), fourthConnectionIdentifier);
    EXPECT_EQ(foundFourthIdentfierInUpdate, updateFoundIdentifiers.end());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, TimepointFilterTest)
{
    using namespace std::chrono_literals;
    ConnectionTracker<std::string> tracker;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = Peer::Proxy::CreateInstance(
        Node::Identifier{ Node::GenerateIdentifier() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetIdentifier();
    ConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetConnectionState(ConnectionState::Disconnected);
    firstConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = Peer::Proxy::CreateInstance(
        Node::Identifier{ Node::GenerateIdentifier() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetIdentifier();
    ConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetConnectionState(ConnectionState::Resolving);
    secondConnectionDetails.SetUpdatedTimepoint(timepoint - 10min);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = Peer::Proxy::CreateInstance(
        Node::Identifier{ Node::GenerateIdentifier() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetIdentifier();
    ConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetConnectionState(ConnectionState::Connected);
    thirdConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const fourthConnectionIdentifier = "4";

    tracker.TrackConnection(firstConnectionIdentifier, firstConnectionDetails);
    tracker.TrackConnection(secondConnectionIdentifier, secondConnectionDetails);
    tracker.TrackConnection(thirdConnectionIdentifier, thirdConnectionDetails);
    tracker.TrackConnection(fourthConnectionIdentifier);

    std::vector<std::string> readFoundIdentifiers;
    tracker.ReadEachConnection(
        [&readFoundIdentifiers](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            readFoundIdentifiers.push_back(id);
            return CallbackIteration::Continue;
        },
        UpdateTimepointFilter::MatchPredicate,
        [&timepoint](TimeUtils::Timepoint const& updated) -> bool
        {
            return (updated < timepoint);
        }
    );

    EXPECT_EQ(readFoundIdentifiers.size(), std::size_t(1));
    auto const secondThirdIdentifierInRead = std::find(
        readFoundIdentifiers.begin(), readFoundIdentifiers.end(), secondConnectionIdentifier);
    EXPECT_NE(secondThirdIdentifierInRead, readFoundIdentifiers.end());

    std::vector<std::string> updateFoundIdentifiers;
    tracker.ReadEachConnection(
        [&updateFoundIdentifiers](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            updateFoundIdentifiers.push_back(id);
            return CallbackIteration::Continue;
        },
        UpdateTimepointFilter::MatchPredicate,
        [&timepoint](TimeUtils::Timepoint const& updated) -> bool
        {
            return (updated == timepoint);
        }
    );

    EXPECT_EQ(updateFoundIdentifiers.size(), std::size_t(2));
    auto const firstThirdIdentifierInUpdate= std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), firstConnectionIdentifier);
    auto const thirdThirdIdentifierInUpdate= std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), thirdConnectionIdentifier);
    EXPECT_NE(firstThirdIdentifierInUpdate, updateFoundIdentifiers.end());
    EXPECT_NE(thirdThirdIdentifierInUpdate, updateFoundIdentifiers.end());
}

//----------------------------------------------------------------------------------------------------------------------