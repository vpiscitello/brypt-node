
//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "../../Utilities/TimeUtils.hpp"
#include "../../Components/Endpoints/ConnectionTracker.hpp"
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

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ServerIdentifier(BryptIdentifier::Generate());

constexpr std::string_view TechnologyName = "Direct";
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35222";
constexpr std::string_view ClientBinding = "*:35223";
constexpr std::string_view ServerEntry = "127.0.0.1:35222";
constexpr std::string_view ClientEntry = "127.0.0.1:35223";

//------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

TEST(ConnectionDetailsSuite, IdentifierTranslateTest)
{
    CConnectionTracker<std::string> tracker;
    
    std::string const connection = "1";
    tracker.TrackConnection(connection);

    auto const spFirstBryptIdentifier = tracker.Translate(connection);
    EXPECT_FALSE(spFirstBryptIdentifier);

    auto const spBryptPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spBryptIdentifier = spBryptPeer->GetBryptIdentifier();

    auto const optFirstConnectionIdentifier = tracker.Translate(*spBryptIdentifier);
    EXPECT_FALSE(optFirstConnectionIdentifier);

    tracker.UpdateOneConnection(connection,
        [] ([[maybe_unused]] auto& details) {
            ASSERT_FALSE(true);
        },
        [this, &spBryptPeer] (
            [[maybe_unused]] std::string_view uri) -> CConnectionDetails<>
        {
            CConnectionDetails<> details(spBryptPeer);
            details.SetConnectionState(ConnectionState::Unknown);
            details.SetMessagingPhase(MessagingPhase::Response);
            return details;
        }
    );
    
    auto const spSecondBryptIdentifier = tracker.Translate(connection);
    auto const optSecondConnectionIdentifier = tracker.Translate(*spBryptIdentifier);
    EXPECT_EQ(spBryptIdentifier, spSecondBryptIdentifier);
    EXPECT_EQ(
        spBryptIdentifier->GetInternalRepresentation(),
        spSecondBryptIdentifier->GetInternalRepresentation());
    EXPECT_EQ(connection, *optSecondConnectionIdentifier);
}

//------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, SingleConnectionTest)
{
    CConnectionTracker<std::string> tracker;
    
    std::string const clientConnectionId = "1";
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    CConnectionDetails<> details(spClientPeer);
    details.SetConnectionState(ConnectionState::Unknown);
    details.SetMessagingPhase(MessagingPhase::Response);

    tracker.TrackConnection(clientConnectionId, details);

    auto const optConnectionId = tracker.Translate(test::ClientIdentifier);
    ASSERT_TRUE(optConnectionId);
    EXPECT_EQ(*optConnectionId, clientConnectionId);

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

//------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, MultipleConnectionsTest)
{
    CConnectionTracker<std::string> tracker;
    
    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetBryptIdentifier();
    CConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetConnectionState(ConnectionState::Unknown);
    firstConnectionDetails.SetMessagingPhase(MessagingPhase::Response);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetBryptIdentifier();
    CConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetConnectionState(ConnectionState::Unknown);
    secondConnectionDetails.SetMessagingPhase(MessagingPhase::Response);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetBryptIdentifier();
    CConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetConnectionState(ConnectionState::Unknown);
    thirdConnectionDetails.SetMessagingPhase(MessagingPhase::Response);

    tracker.TrackConnection(firstConnectionIdentifier, firstConnectionDetails);
    tracker.TrackConnection(secondConnectionIdentifier, secondConnectionDetails);
    tracker.TrackConnection(thirdConnectionIdentifier, thirdConnectionDetails);

    auto const optConnectionIdentifier = tracker.Translate(*spSecondNodeIdentifier);
    ASSERT_TRUE(optConnectionIdentifier);
    EXPECT_EQ(*optConnectionIdentifier, secondConnectionIdentifier);

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

//------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, ConnectionStateFilterTest)
{
    using namespace std::chrono_literals;
    CConnectionTracker<std::string> tracker;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();

    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetBryptIdentifier();
    CConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetMessageSequenceNumber(std::uint32_t(57));
    firstConnectionDetails.SetConnectionState(ConnectionState::Disconnected);
    firstConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
    firstConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetBryptIdentifier();
    CConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetMessageSequenceNumber(std::uint32_t(12));
    secondConnectionDetails.SetConnectionState(ConnectionState::Flagged);
    secondConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
    secondConnectionDetails.SetUpdatedTimepoint(timepoint - 10min);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetBryptIdentifier();
    CConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetMessageSequenceNumber(std::uint32_t(492));
    thirdConnectionDetails.SetConnectionState(ConnectionState::Connected);
    thirdConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
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
        ConnectionStateFilter::Disconnected | ConnectionStateFilter::Flagged
    );

    EXPECT_EQ(updateFoundIdentifiers.size(), std::size_t(2));
    auto const foundFirstIdentifierInUpdate = std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), firstConnectionIdentifier);
    auto const foundSecondIdentifierInUpdate = std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), secondConnectionIdentifier);
    EXPECT_NE(foundFirstIdentifierInUpdate, updateFoundIdentifiers.end());
    EXPECT_NE(foundSecondIdentifierInUpdate, updateFoundIdentifiers.end());
}

//------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, PromotionFilterTest)
{
    using namespace std::chrono_literals;
    CConnectionTracker<std::string> tracker;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetBryptIdentifier();
    CConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetMessageSequenceNumber(std::uint32_t(57));
    firstConnectionDetails.SetConnectionState(ConnectionState::Disconnected);
    firstConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
    firstConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetBryptIdentifier();
    CConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetMessageSequenceNumber(std::uint32_t(12));
    secondConnectionDetails.SetConnectionState(ConnectionState::Flagged);
    secondConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
    secondConnectionDetails.SetUpdatedTimepoint(timepoint - 10min);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetBryptIdentifier();
    CConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetMessageSequenceNumber(std::uint32_t(492));
    thirdConnectionDetails.SetConnectionState(ConnectionState::Connected);
    thirdConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
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

//------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, MessageSequenceFilterTest)
{
    using namespace std::chrono_literals;
    CConnectionTracker<std::string> tracker;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetBryptIdentifier();
    CConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetMessageSequenceNumber(std::uint32_t(57));
    firstConnectionDetails.SetConnectionState(ConnectionState::Disconnected);
    firstConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
    firstConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetBryptIdentifier();
    CConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetMessageSequenceNumber(std::uint32_t(12));
    secondConnectionDetails.SetConnectionState(ConnectionState::Flagged);
    secondConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
    secondConnectionDetails.SetUpdatedTimepoint(timepoint - 10min);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetBryptIdentifier();
    CConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetMessageSequenceNumber(std::uint32_t(492));
    thirdConnectionDetails.SetConnectionState(ConnectionState::Connected);
    thirdConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
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
        MessageSequenceFilter::MatchPredicate,
        [](std::uint32_t sequenceNumber) -> bool
        {
            return (sequenceNumber > 100);
        }
    );

    EXPECT_EQ(readFoundIdentifiers.size(), std::size_t(1));
    auto const foundThirdIdentifierInRead = std::find(
        readFoundIdentifiers.begin(), readFoundIdentifiers.end(), thirdConnectionIdentifier);
    EXPECT_NE(foundThirdIdentifierInRead, readFoundIdentifiers.end());

    std::vector<std::string> updateFoundIdentifiers;
    tracker.ReadEachConnection(
        [&updateFoundIdentifiers](auto const& id, [[maybe_unused]] auto const& optDetails) -> CallbackIteration
        {
            updateFoundIdentifiers.push_back(id);
            return CallbackIteration::Continue;
        },
        MessageSequenceFilter::MatchPredicate,
        [](std::uint32_t sequenceNumber) -> bool
        {
            return (sequenceNumber < 100);
        }
    );
    EXPECT_EQ(updateFoundIdentifiers.size(), std::size_t(2));
    auto const foundFirstIdentifierInUpdate = std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), firstConnectionIdentifier);
    auto const foundSecondIdentifierInUpdate = std::find(
        updateFoundIdentifiers.begin(), updateFoundIdentifiers.end(), secondConnectionIdentifier);
    EXPECT_NE(foundFirstIdentifierInUpdate, updateFoundIdentifiers.end());
    EXPECT_NE(foundSecondIdentifierInUpdate, updateFoundIdentifiers.end());
}

//------------------------------------------------------------------------------------------------

TEST(ConnectionTrackerSuite, TimepointFilterTest)
{
    using namespace std::chrono_literals;
    CConnectionTracker<std::string> tracker;

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::string const firstConnectionIdentifier = "1";
    auto const spFirstPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spFirstNodeIdentifier = spFirstPeer->GetBryptIdentifier();
    CConnectionDetails<> firstConnectionDetails(spFirstPeer);
    firstConnectionDetails.SetMessageSequenceNumber(std::uint32_t(57));
    firstConnectionDetails.SetConnectionState(ConnectionState::Disconnected);
    firstConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
    firstConnectionDetails.SetUpdatedTimepoint(timepoint);

    std::string const secondConnectionIdentifier = "2";
    auto const spSecondPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spSecondNodeIdentifier= spSecondPeer->GetBryptIdentifier();
    CConnectionDetails<> secondConnectionDetails(spSecondPeer);
    secondConnectionDetails.SetMessageSequenceNumber(std::uint32_t(12));
    secondConnectionDetails.SetConnectionState(ConnectionState::Flagged);
    secondConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
    secondConnectionDetails.SetUpdatedTimepoint(timepoint - 10min);

    std::string const thirdConnectionIdentifier = "3";
    auto const spThirdPeer = std::make_shared<CBryptPeer>(
        BryptIdentifier::CContainer{ BryptIdentifier::Generate() });
    auto const spThirdNodeIdentifier = spThirdPeer->GetBryptIdentifier();
    CConnectionDetails<> thirdConnectionDetails(spThirdPeer);
    thirdConnectionDetails.SetMessageSequenceNumber(std::uint32_t(492));
    thirdConnectionDetails.SetConnectionState(ConnectionState::Connected);
    thirdConnectionDetails.SetMessagingPhase(MessagingPhase::Response);
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

//------------------------------------------------------------------------------------------------