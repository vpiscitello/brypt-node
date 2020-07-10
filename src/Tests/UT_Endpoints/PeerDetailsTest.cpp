
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
#include "../../Utilities/TimeUtils.hpp"
#include "../../Components/Endpoints/PeerDetailsMap.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <chrono>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr NodeUtils::NodeIdType ServerId = 0x12345678;
constexpr NodeUtils::NodeIdType ClientId = 0xFFFFFFFF;
constexpr std::string_view TechnologyName = "Direct";
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";
constexpr std::string_view ClientEntry = "127.0.0.1:35217";

//------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

TEST(PeerDetailsSuite, PeerMapSingleNodeTest)
{
    CPeerDetailsMap<std::string> peers;
    
    std::string const clientConnectionId = "1";
    CPeerDetails<> const details(
        test::ClientId,
        ConnectionState::Unknown,
        MessagingPhase::Response);

    peers.TrackConnection(clientConnectionId, details);

    auto const optConnectionId = peers.Translate(test::ClientId);
    ASSERT_TRUE(optConnectionId);
    EXPECT_EQ(*optConnectionId, clientConnectionId);

    auto const optNodeId = peers.Translate(clientConnectionId);
    ASSERT_TRUE(optNodeId);
    EXPECT_EQ(*optNodeId, test::ClientId);

    bool const bFirstNodeReadFound = peers.ReadOnePeer(clientConnectionId,
        [](auto const& details)
        {
            EXPECT_EQ(details.GetConnectionState(), ConnectionState::Unknown);
        }
    );
    EXPECT_TRUE(bFirstNodeReadFound);

    bool const bFirstNodeUpdateFound = peers.UpdateOnePeer(clientConnectionId,
        [](auto& details)
        {
            details.SetConnectionState(ConnectionState::Connected);
        }
    );
    EXPECT_TRUE(bFirstNodeUpdateFound);

    bool const bSecondNodeReadFound = peers.ReadOnePeer(clientConnectionId,
        [](auto const& details)
        {
            EXPECT_EQ(details.GetConnectionState(), ConnectionState::Connected);
        }
    );
    EXPECT_TRUE(bSecondNodeReadFound);

    peers.UntrackConnection(clientConnectionId);

    bool const bThirdNodeReadFound = peers.ReadOnePeer(clientConnectionId,
        []([[maybe_unused]] auto const& details){}
    );
    EXPECT_FALSE(bThirdNodeReadFound);
}

//------------------------------------------------------------------------------------------------

TEST(PeerDetailsSuite, PeerMapMultipleNodeTest)
{
    CPeerDetailsMap<std::string> peers;
    
    std::string const firstClientConnectionId = "1";
    NodeUtils::NodeIdType const firstClientNodeId = 0x00000001;
    CPeerDetails<> const firstClientInformation(
        firstClientNodeId,
        ConnectionState::Unknown,
        MessagingPhase::Response);

    std::string const secondClientConnectionId = "2";
    NodeUtils::NodeIdType const secondClientNodeId = 0x00000002;
    CPeerDetails<> const secondClientInformation(
        secondClientNodeId,
        ConnectionState::Unknown,
        MessagingPhase::Response);

    std::string const thirdClientConnectionId = "3";
    NodeUtils::NodeIdType const thirdClientNodeId = 0x00000003;
    CPeerDetails<> const thirdClientInformation(
        thirdClientNodeId,
        ConnectionState::Unknown,
        MessagingPhase::Response);

    peers.TrackConnection(firstClientConnectionId, firstClientInformation);
    peers.TrackConnection(secondClientConnectionId, secondClientInformation);
    peers.TrackConnection(thirdClientConnectionId, thirdClientInformation);

    auto const optConnectionId = peers.Translate(secondClientNodeId);
    ASSERT_TRUE(optConnectionId);
    EXPECT_EQ(*optConnectionId, secondClientConnectionId);

    auto const optNodeId = peers.Translate(firstClientConnectionId);
    ASSERT_TRUE(optNodeId);
    EXPECT_EQ(*optNodeId, firstClientNodeId);

    bool const bFirstNodeReadFound = peers.ReadOnePeer(thirdClientConnectionId,
        [](auto const& details)
        {
            EXPECT_EQ(details.GetConnectionState(), ConnectionState::Unknown);
        }
    );
    EXPECT_TRUE(bFirstNodeReadFound);

    bool const bFirstNodeUpdateFound = peers.UpdateOnePeer(secondClientConnectionId,
        [](auto& details)
        {
            details.SetConnectionState(ConnectionState::Disconnected);
        }
    );
    EXPECT_TRUE(bFirstNodeUpdateFound);

    bool const bSecondNodeReadFound = peers.ReadOnePeer(secondClientConnectionId,
        [](auto const& details)
        {
            EXPECT_EQ(details.GetConnectionState(), ConnectionState::Disconnected);
        }
    );
    EXPECT_TRUE(bSecondNodeReadFound);

    std::uint32_t updateCounter = 0;
    peers.UpdateEachPeer(
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

    peers.UntrackConnection(firstClientConnectionId);

    std::uint32_t readCounter = 0;
    peers.ReadEachPeer(
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

//------------------------------------------------------------------------------------------------

TEST(PeerDetailsSuite, PeerMapConnectionStateFilterTest)
{
    using namespace std::chrono_literals;
    CPeerDetailsMap<std::string> peers;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstClientConnectionId = "1";
    NodeUtils::NodeIdType const firstClientNodeId = 0x00000001;
    CPeerDetails<> const firstClientInformation(
        firstClientNodeId,
        timepoint,
        std::uint32_t(57),
        ConnectionState::Disconnected,
        MessagingPhase::Response);

    std::string const secondClientConnectionId = "2";
    NodeUtils::NodeIdType const secondClientNodeId = 0x00000002;
    CPeerDetails<> const secondClientInformation(
        secondClientNodeId,
        timepoint - 10min,
        std::uint32_t(12),
        ConnectionState::Flagged,
        MessagingPhase::Response);

    std::string const thirdClientConnectionId = "3";
    NodeUtils::NodeIdType const thirdClientNodeId = 0x00000003;
    CPeerDetails<> const thirdClientInformation(
        thirdClientNodeId,
        timepoint,
        std::uint32_t(492),
        ConnectionState::Connected,
        MessagingPhase::Response);

    std::string const fourthClientConnectionId = "4";

    peers.TrackConnection(firstClientConnectionId, firstClientInformation);
    peers.TrackConnection(secondClientConnectionId, secondClientInformation);
    peers.TrackConnection(thirdClientConnectionId, thirdClientInformation);
    peers.TrackConnection(fourthClientConnectionId);

    std::vector<std::string> readFoundIds;
    peers.ReadEachPeer(
        [&readFoundIds](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            readFoundIds.push_back(id);
            return CallbackIteration::Continue;
        },
        ConnectionStateFilter::Connected
    );
    auto const foundThirdIdInRead = std::find(readFoundIds.begin(), readFoundIds.end(), thirdClientConnectionId);
    EXPECT_EQ(readFoundIds.size(), std::size_t(1));
    EXPECT_NE(foundThirdIdInRead, readFoundIds.end());

    std::vector<std::string> updateFoundIds;
    peers.UpdateEachPeer(
        [&updateFoundIds](auto const& id, [[maybe_unused]] auto& optDetails) -> CallbackIteration
        {
            updateFoundIds.push_back(id);
            return CallbackIteration::Continue;
        },
        ConnectionStateFilter::Disconnected | ConnectionStateFilter::Flagged
    );
    auto const foundFirstIdInUpdate = std::find(updateFoundIds.begin(), updateFoundIds.end(), firstClientConnectionId);
    auto const foundSecondIdInUpdate = std::find(updateFoundIds.begin(), updateFoundIds.end(), secondClientConnectionId);
    EXPECT_EQ(updateFoundIds.size(), std::size_t(2));
    EXPECT_NE(foundFirstIdInUpdate, updateFoundIds.end());
    EXPECT_NE(foundSecondIdInUpdate, updateFoundIds.end());
}

//------------------------------------------------------------------------------------------------

TEST(PeerDetailsSuite, PeerMapPromotionFilterTest)
{
    using namespace std::chrono_literals;
    CPeerDetailsMap<std::string> peers;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstClientConnectionId = "1";
    NodeUtils::NodeIdType const firstClientNodeId = 0x00000001;
    CPeerDetails<> const firstClientInformation(
        firstClientNodeId,
        timepoint,
        std::uint32_t(57),
        ConnectionState::Disconnected,
        MessagingPhase::Response);

    std::string const secondClientConnectionId = "2";
    NodeUtils::NodeIdType const secondClientNodeId = 0x00000002;
    CPeerDetails<> const secondClientInformation(
        secondClientNodeId,
        timepoint - 10min,
        std::uint32_t(12),
        ConnectionState::Flagged,
        MessagingPhase::Response);

    std::string const thirdClientConnectionId = "3";
    NodeUtils::NodeIdType const thirdClientNodeId = 0x00000003;
    CPeerDetails<> const thirdClientInformation(
        thirdClientNodeId,
        timepoint,
        std::uint32_t(492),
        ConnectionState::Connected,
        MessagingPhase::Response);

    std::string const fourthClientConnectionId = "4";

    peers.TrackConnection(firstClientConnectionId, firstClientInformation);
    peers.TrackConnection(secondClientConnectionId, secondClientInformation);
    peers.TrackConnection(thirdClientConnectionId, thirdClientInformation);
    peers.TrackConnection(fourthClientConnectionId);

    std::vector<std::string> readFoundIds;
    peers.ReadEachPeer(
        [&readFoundIds](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            readFoundIds.push_back(id);
            return CallbackIteration::Continue;
        },
        PromotionStateFilter::Unpromoted
    );
    auto const foundFourthIdInRead= std::find(readFoundIds.begin(), readFoundIds.end(), fourthClientConnectionId);
    EXPECT_EQ(readFoundIds.size(), std::size_t(1));
    EXPECT_NE(foundFourthIdInRead, readFoundIds.end());

    std::vector<std::string> updateFoundIds;
    peers.ReadEachPeer(
        [&updateFoundIds](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            updateFoundIds.push_back(id);
            return CallbackIteration::Continue;
        },
        PromotionStateFilter::Promoted
    );
    auto const foundFourthIdInUpdate = std::find(updateFoundIds.begin(), updateFoundIds.end(), fourthClientConnectionId);
    EXPECT_EQ(updateFoundIds.size(), std::size_t(3));
    EXPECT_EQ(foundFourthIdInUpdate, updateFoundIds.end());
}

//------------------------------------------------------------------------------------------------

TEST(PeerDetailsSuite, PeerMapMessageSequenceFilterTest)
{
    using namespace std::chrono_literals;
    CPeerDetailsMap<std::string> peers;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstClientConnectionId = "1";
    NodeUtils::NodeIdType const firstClientNodeId = 0x00000001;
    CPeerDetails<> const firstClientInformation(
        firstClientNodeId,
        timepoint,
        std::uint32_t(57),
        ConnectionState::Disconnected,
        MessagingPhase::Response);

    std::string const secondClientConnectionId = "2";
    NodeUtils::NodeIdType const secondClientNodeId = 0x00000002;
    CPeerDetails<> const secondClientInformation(
        secondClientNodeId,
        timepoint - 10min,
        std::uint32_t(12),
        ConnectionState::Flagged,
        MessagingPhase::Response);

    std::string const thirdClientConnectionId = "3";
    NodeUtils::NodeIdType const thirdClientNodeId = 0x00000003;
    CPeerDetails<> const thirdClientInformation(
        thirdClientNodeId,
        timepoint,
        std::uint32_t(492),
        ConnectionState::Connected,
        MessagingPhase::Response);

    std::string const fourthClientConnectionId = "4";

    peers.TrackConnection(firstClientConnectionId, firstClientInformation);
    peers.TrackConnection(secondClientConnectionId, secondClientInformation);
    peers.TrackConnection(thirdClientConnectionId, thirdClientInformation);
    peers.TrackConnection(fourthClientConnectionId);

    std::vector<std::string> readFoundIds;
    peers.ReadEachPeer(
        [&readFoundIds](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            readFoundIds.push_back(id);
            return CallbackIteration::Continue;
        },
        MessageSequenceFilter::MatchPredicate,
        [](std::uint32_t sequenceNumber) -> bool
        {
            return (sequenceNumber > 100);
        }
    );
    auto const foundThirdIdInRead= std::find(readFoundIds.begin(), readFoundIds.end(), thirdClientConnectionId);
    EXPECT_EQ(readFoundIds.size(), std::size_t(1));
    EXPECT_NE(foundThirdIdInRead, readFoundIds.end());

    std::vector<std::string> updateFoundIds;
    peers.ReadEachPeer(
        [&updateFoundIds](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            updateFoundIds.push_back(id);
            return CallbackIteration::Continue;
        },
        MessageSequenceFilter::MatchPredicate,
        [](std::uint32_t sequenceNumber) -> bool
        {
            return (sequenceNumber < 100);
        }
    );
    auto const foundFirstIdInUpdate = std::find(updateFoundIds.begin(), updateFoundIds.end(), firstClientConnectionId);
    auto const foundSecondIdInUpdate = std::find(updateFoundIds.begin(), updateFoundIds.end(), secondClientConnectionId);
    EXPECT_EQ(updateFoundIds.size(), std::size_t(2));
    EXPECT_NE(foundFirstIdInUpdate, updateFoundIds.end());
    EXPECT_NE(foundSecondIdInUpdate, updateFoundIds.end());
}

//------------------------------------------------------------------------------------------------

TEST(PeerDetailsSuite, PeerMapTimepointFilterTest)
{
    using namespace std::chrono_literals;
    CPeerDetailsMap<std::string> peers;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstClientConnectionId = "1";
    NodeUtils::NodeIdType const firstClientNodeId = 0x00000001;
    CPeerDetails<> const firstClientInformation(
        firstClientNodeId,
        timepoint,
        std::uint32_t(57),
        ConnectionState::Disconnected,
        MessagingPhase::Response);

    std::string const secondClientConnectionId = "2";
    NodeUtils::NodeIdType const secondClientNodeId = 0x00000002;
    CPeerDetails<> const secondClientInformation(
        secondClientNodeId,
        timepoint - 10min,
        std::uint32_t(12),
        ConnectionState::Flagged,
        MessagingPhase::Response);

    std::string const thirdClientConnectionId = "3";
    NodeUtils::NodeIdType const thirdClientNodeId = 0x00000003;
    CPeerDetails<> const thirdClientInformation(
        thirdClientNodeId,
        timepoint,
        std::uint32_t(492),
        ConnectionState::Connected,
        MessagingPhase::Response);

    std::string const fourthClientConnectionId = "4";

    peers.TrackConnection(firstClientConnectionId, firstClientInformation);
    peers.TrackConnection(secondClientConnectionId, secondClientInformation);
    peers.TrackConnection(thirdClientConnectionId, thirdClientInformation);
    peers.TrackConnection(fourthClientConnectionId);

    std::vector<std::string> readFoundIds;
    peers.ReadEachPeer(
        [&readFoundIds](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            readFoundIds.push_back(id);
            return CallbackIteration::Continue;
        },
        UpdateTimepointFilter::MatchPredicate,
        [&timepoint](TimeUtils::Timepoint const& updated) -> bool
        {
            return (updated < timepoint);
        }
    );
    auto const secondThirdIdInRead= std::find(readFoundIds.begin(), readFoundIds.end(), secondClientConnectionId);
    EXPECT_EQ(readFoundIds.size(), std::size_t(1));
    EXPECT_NE(secondThirdIdInRead, readFoundIds.end());

    std::vector<std::string> updateFoundIds;
    peers.ReadEachPeer(
        [&updateFoundIds](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            updateFoundIds.push_back(id);
            return CallbackIteration::Continue;
        },
        UpdateTimepointFilter::MatchPredicate,
        [&timepoint](TimeUtils::Timepoint const& updated) -> bool
        {
            return (updated == timepoint);
        }
    );
    auto const firstThirdIdInUpdate= std::find(updateFoundIds.begin(), updateFoundIds.end(), firstClientConnectionId);
    auto const thirdThirdIdInUpdate= std::find(updateFoundIds.begin(), updateFoundIds.end(), thirdClientConnectionId);
    EXPECT_EQ(updateFoundIds.size(), std::size_t(2));
    EXPECT_NE(firstThirdIdInUpdate, updateFoundIds.end());
    EXPECT_NE(thirdThirdIdInUpdate, updateFoundIds.end());
}

//------------------------------------------------------------------------------------------------