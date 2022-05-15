
//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Network/ConnectionTracker.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/ResolutionService.hpp"
#include "Utilities/NodeUtils.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <chrono>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace test {
//----------------------------------------------------------------------------------------------------------------------

using ConnectionIdentifier = std::string;
using PeerConnection = std::tuple<ConnectionIdentifier, Node::Identifier, std::shared_ptr<Peer::Proxy>>;

auto const ClientIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class ConnectionTrackerSuite : public testing::Test
{
protected:
    void SetUp() override
    {
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

        m_spTaskService = std::make_shared<Scheduler::TaskService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTaskService);

        m_spTrackingService = std::make_shared<Awaitable::TrackingService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTrackingService);

        m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
        m_spServiceProvider->Register(m_spEventPublisher);

        m_spNodeState = std::make_shared<NodeState>(test::ClientIdentifier, Network::ProtocolSet{});
        m_spServiceProvider->Register(m_spNodeState);

        m_spMessageProcessor = std::make_shared<Network::Test::MessageProcessor>(test::ClientIdentifier);
        m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);
        
        m_spResolutionService = std::make_shared<Network::Test::SingleResolutionService>(
            test::ClientIdentifier, m_spMessageProcessor.get(), m_spServiceProvider);
        m_spServiceProvider->Register<IResolutionService>(m_spResolutionService);
    }

    void GeneratePeerConnections(std::size_t generate)
    {
        EXPECT_TRUE(m_tracker.IsEmpty());
        EXPECT_EQ(m_tracker.GetSize(), std::size_t{ 0 });

        std::ranges::generate_n(std::back_insert_iterator(m_connections), generate, [&, generated = 0] () mutable { 
            auto const identifier = Node::Identifier{ Node::GenerateIdentifier() };
            auto const connection = std::make_tuple(
                std::to_string(generated),
                identifier,
                Peer::Proxy::CreateInstance(identifier, m_spServiceProvider));
            ++generated;
            return connection;
        });

        ASSERT_EQ(m_connections.size(), generate);

        for (auto const& [connection, spPeerIdentifier, spProxy] : m_connections) {
            ConnectionDetails<> details{ spProxy };
            m_tracker.TrackConnection(connection, details);
        }
     
        EXPECT_FALSE(m_tracker.IsEmpty());
        EXPECT_EQ(m_tracker.GetSize(), generate);
    }

    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Network::Test::MessageProcessor> m_spMessageProcessor;
    std::shared_ptr<Network::Test::SingleResolutionService> m_spResolutionService;

    std::vector<test::PeerConnection> m_connections;
    ConnectionTracker<test::ConnectionIdentifier> m_tracker;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, IdentiferMappingsTest)
{
    std::string const connection = "1";
    m_tracker.TrackConnection(connection);

    // The connection identifier should not be mapped to a node identifier before one has been associated with it. 
    {
        auto const spPeerIdentifier = m_tracker.Translate(connection);
        EXPECT_FALSE(spPeerIdentifier);
    }

    auto const spProxy = Peer::Proxy::CreateInstance(Node::Identifier{ Node::GenerateIdentifier() }, m_spServiceProvider);
    auto const spPeerIdentifier = spProxy->GetIdentifier();

    // The peer identifier should not be mapped to a connection identifier before one has been associated with it. 
    {
        auto const connection = m_tracker.Translate(*spPeerIdentifier);
        EXPECT_TRUE(connection.empty());
    }

    // Associate the connection with the generated proxy. 
    m_tracker.UpdateOneConnection(connection,
        [] ([[maybe_unused]] auto& details) { ASSERT_FALSE(true); },
        [this, &spProxy] ([[maybe_unused]] Network::RemoteAddress const& address) -> ConnectionDetails<> {
            ConnectionDetails<> details(spProxy);
            details.SetConnectionState(Network::Connection::State::Connected);
            return details;
        });
    
    // The identifiers should now be mapped and translatable. 
    auto const translated = m_tracker.Translate(*spPeerIdentifier);
    EXPECT_EQ(translated, connection);

    auto const spTranslatedIdentifier = m_tracker.Translate(connection);
    EXPECT_EQ(*spPeerIdentifier, *spTranslatedIdentifier);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, TranslateIdentifiersTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount);

    for (auto const& [connection, identifier, spProxy] : m_connections) {
        auto const translated = m_tracker.Translate(identifier);
        EXPECT_EQ(translated, connection);

        auto const spTranslatedIdentifier = m_tracker.Translate(connection);
        ASSERT_TRUE(spTranslatedIdentifier);
        EXPECT_EQ(*spTranslatedIdentifier, identifier);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, ReadOneConnectionTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount);

    for (auto const& [connection, spPeerIdentifier, spProxy] : m_connections) {
        bool const found = m_tracker.ReadOneConnection(connection, [] (auto const& details) {
            EXPECT_EQ(details.GetConnectionState(), Network::Connection::State::Resolving);
        });
        EXPECT_TRUE(found);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, ReadOneUnknownConnectionTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount);
    bool const found = m_tracker.ReadOneConnection("unknown", [] (auto const&) { });
    EXPECT_FALSE(found);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, ReadEachConnectionTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount);

    std::size_t counter = 0;
    m_tracker.ReadEachConnection([&] (test::ConnectionIdentifier const& id, auto const& optDetails) -> CallbackIteration {
        auto const itr = std::ranges::find_if(m_connections, [&id] (test::PeerConnection const& entry) {
            return std::get<0>(entry) == id;
        });
        EXPECT_NE(itr, m_connections.end());

        EXPECT_TRUE(optDetails);
        EXPECT_EQ(optDetails->GetConnectionState(), Network::Connection::State::Resolving);
        ++counter;
        return CallbackIteration::Continue;
    });
    EXPECT_EQ(counter, GenerateCount);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, ReadEachConnectionStopTest)
{
    constexpr std::size_t GenerateCount = 5;
    constexpr std::size_t StopPosition = 3;
    GeneratePeerConnections(GenerateCount);

    std::size_t counter = 0;
    m_tracker.ReadEachConnection([&] (test::ConnectionIdentifier const&, auto const& optDetails) -> CallbackIteration {
        EXPECT_TRUE(optDetails);
        EXPECT_EQ(optDetails->GetConnectionState(), Network::Connection::State::Resolving);
        ++counter;
        return (counter < 3) ? CallbackIteration::Continue : CallbackIteration::Stop;
    });
    EXPECT_EQ(counter, StopPosition);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, UpdateOneConnectionTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount);

    for (auto const& [connection, spPeerIdentifier, spProxy] : m_connections) {
        bool const isTrackerUpdated = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Connected);
        });
        EXPECT_TRUE(isTrackerUpdated);

        bool const isUpdateFound = m_tracker.ReadOneConnection(connection, [&] (auto const& details) {
            EXPECT_EQ(details.GetConnectionState(), Network::Connection::State::Connected);
        });
        EXPECT_TRUE(isUpdateFound);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, UpdateOneUnknownConnectionTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount);
    bool const found = m_tracker.ReadOneConnection("unknown", [] (auto const&) { });
    EXPECT_FALSE(found);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, UpdateEachConnectionTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount);

    {
        std::size_t counter = 0;
        m_tracker.UpdateEachConnection([&] (test::ConnectionIdentifier const& id, auto& optDetails) -> CallbackIteration {
            auto const itr = std::ranges::find_if(m_connections, [&id] (test::PeerConnection const& entry) {
                return std::get<0>(entry) == id;
            });
            EXPECT_NE(itr, m_connections.end());

            EXPECT_TRUE(optDetails);
            optDetails->SetConnectionState(Network::Connection::State::Unknown);
            ++counter;
            return CallbackIteration::Continue;
        });
        EXPECT_EQ(counter, GenerateCount);
    }

    {
        std::size_t counter = 0;
        m_tracker.ReadEachConnection([&] (test::ConnectionIdentifier const&, auto const& optDetails) -> CallbackIteration {
            EXPECT_TRUE(optDetails);
            EXPECT_EQ(optDetails->GetConnectionState(), Network::Connection::State::Unknown);
            ++counter;
            return CallbackIteration::Continue;
        });
        EXPECT_EQ(counter, GenerateCount);     
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, UpdateEachConnectionStopTest)
{
    constexpr std::size_t GenerateCount = 5;
    constexpr std::size_t StopPosition = 3;
    GeneratePeerConnections(GenerateCount);

    {
        std::size_t counter = 0;
        m_tracker.UpdateEachConnection([&] (test::ConnectionIdentifier const&, auto& optDetails) -> CallbackIteration {
            EXPECT_TRUE(optDetails);
            optDetails->SetConnectionState(Network::Connection::State::Unknown);
            ++counter;
            return (counter < 3) ? CallbackIteration::Continue : CallbackIteration::Stop;
        });
        EXPECT_EQ(counter, StopPosition);
    }

    {
        std::size_t counter = 0;
        m_tracker.ReadEachConnection([&] (test::ConnectionIdentifier const&, auto const& optDetails) -> CallbackIteration {
            EXPECT_TRUE(optDetails);
            auto const expected = (counter < 3) ? 
                Network::Connection::State::Unknown : Network::Connection::State::Resolving;
            EXPECT_EQ(optDetails->GetConnectionState(), expected);
            ++counter;
            return CallbackIteration::Continue;
        });
        EXPECT_EQ(counter, GenerateCount);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, UntrackConnectionTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount);

    auto const [untracked, identifier, spProxy] = m_connections[2];

    {
        bool const found = m_tracker.ReadOneConnection(untracked, [] (auto const& details) {
            EXPECT_EQ(details.GetConnectionState(), Network::Connection::State::Resolving);
        });
        EXPECT_TRUE(found);
    }

    m_tracker.UntrackConnection(untracked);

    {
        bool const found = m_tracker.ReadOneConnection(untracked, [] (auto const& details) {
            EXPECT_EQ(details.GetConnectionState(), Network::Connection::State::Resolving);
        });
        EXPECT_FALSE(found);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, ResetConnectionsTest)
{
    constexpr std::size_t GenerateCount = 5;
    GeneratePeerConnections(GenerateCount); 

    EXPECT_EQ(m_tracker.GetSize(), GenerateCount);

    m_tracker.ResetConnections([&] (test::ConnectionIdentifier const& id, auto&) -> CallbackIteration {
        auto const itr = std::ranges::find_if(m_connections, [&id] (test::PeerConnection const& entry) {
            return std::get<0>(entry) == id;
        });
        EXPECT_NE(itr, m_connections.end());
        return CallbackIteration::Continue; 
    });

    EXPECT_EQ(m_tracker.GetSize(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, ConnectionStateFilterTest)
{
    using namespace std::chrono_literals;

    constexpr std::size_t GenerateCount = 3;
    GeneratePeerConnections(GenerateCount); 

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::vector<test::ConnectionIdentifier> expectedConnected;
    std::vector<test::ConnectionIdentifier> expectedOther;
    {
        auto const [connection, identifier, spProxy] = m_connections[0];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Disconnected);
            details.SetUpdatedTimepoint(timepoint);
        });
        EXPECT_TRUE(found);
        expectedOther.emplace_back(connection);
    }

    {
        auto const [connection, identifier, spProxy] = m_connections[1];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Unknown);
            details.SetUpdatedTimepoint(timepoint - 10min);
        });
        EXPECT_TRUE(found);
        expectedOther.emplace_back(connection);
    }

    {
        auto const [connection, identifier, spProxy] = m_connections[2];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Connected);
            details.SetUpdatedTimepoint(timepoint);
        });
        EXPECT_TRUE(found);
        expectedConnected.emplace_back(connection);
    }

    {
        test::ConnectionIdentifier const resolving = "resolving";
        m_tracker.TrackConnection(resolving);
    }

    std::vector<test::ConnectionIdentifier> connected;
    m_tracker.ReadEachConnection([&connected] (auto const& id, auto const&) -> CallbackIteration {
        connected.push_back(id);
        return CallbackIteration::Continue;
    },
    ConnectionStateFilter::Connected);

    EXPECT_TRUE(std::ranges::equal(connected, expectedConnected));

    std::vector<std::string> other;
    m_tracker.UpdateEachConnection([&other] (auto const& id, auto&) -> CallbackIteration {
        other.push_back(id);
        return CallbackIteration::Continue;
    },
    ConnectionStateFilter::Disconnected | ConnectionStateFilter::Unknown);

    EXPECT_TRUE(std::ranges::equal(other, expectedOther));
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, PromotionFilterTest)
{
    using namespace std::chrono_literals;

    constexpr std::size_t GenerateCount = 3;
    GeneratePeerConnections(GenerateCount); 

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::vector<test::ConnectionIdentifier> expectedPromoted;
    std::vector<test::ConnectionIdentifier> expectedUnpromoted;
    {
        auto const [connection, identifier, spProxy] = m_connections[0];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Disconnected);
            details.SetUpdatedTimepoint(timepoint);
        });
        EXPECT_TRUE(found);
        expectedPromoted.emplace_back(connection);
    }

    {
        auto const [connection, identifier, spProxy] = m_connections[1];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Unknown);
            details.SetUpdatedTimepoint(timepoint - 10min);
        });
        EXPECT_TRUE(found);
        expectedPromoted.emplace_back(connection);
    }

    {
        auto const [connection, identifier, spProxy] = m_connections[2];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Connected);
            details.SetUpdatedTimepoint(timepoint);
        });
        EXPECT_TRUE(found);
        expectedPromoted.emplace_back(connection);
    }

    {
        test::ConnectionIdentifier const resolving = "resolving";
        m_tracker.TrackConnection(resolving);
        expectedUnpromoted.emplace_back(resolving);
    }

    std::vector<test::ConnectionIdentifier> promoted;
    m_tracker.ReadEachConnection([&promoted] (auto const& id, auto const&) -> CallbackIteration {
        promoted.push_back(id);
        return CallbackIteration::Continue;
    },
    PromotionStateFilter::Promoted);

    EXPECT_TRUE(std::ranges::equal(promoted, expectedPromoted));

    std::vector<std::string> unpromoted;
    m_tracker.UpdateEachConnection([&unpromoted] (auto const& id, auto&) -> CallbackIteration {
        unpromoted.push_back(id);
        return CallbackIteration::Continue;
    },
    PromotionStateFilter::Unpromoted);

    EXPECT_TRUE(std::ranges::equal(unpromoted, expectedUnpromoted));
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectionTrackerSuite, TimepointFilterTest)
{
    using namespace std::chrono_literals;

    constexpr std::size_t GenerateCount = 3;
    GeneratePeerConnections(GenerateCount); 

    TimeUtils::Timepoint timepoint = TimeUtils::GetSystemTimepoint();
    
    std::vector<test::ConnectionIdentifier> expectedActive;
    std::vector<test::ConnectionIdentifier> expectedInactive;
    {
        auto const [connection, identifier, spProxy] = m_connections[0];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Disconnected);
            details.SetUpdatedTimepoint(timepoint);
        });
        EXPECT_TRUE(found);
        expectedActive.emplace_back(connection);
    }

    {
        auto const [connection, identifier, spProxy] = m_connections[1];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Unknown);
            details.SetUpdatedTimepoint(timepoint - 10min);
        });
        EXPECT_TRUE(found);
        expectedInactive.emplace_back(connection);
    }

    {
        auto const [connection, identifier, spProxy] = m_connections[2];
        bool const found = m_tracker.UpdateOneConnection(connection, [&] (auto& details) {
            details.SetConnectionState(Network::Connection::State::Connected);
            details.SetUpdatedTimepoint(timepoint);
        });
        EXPECT_TRUE(found);
        expectedActive.emplace_back(connection);
    }

    {
        test::ConnectionIdentifier const resolving = "resolving";
        m_tracker.TrackConnection(resolving);
    }

    std::vector<test::ConnectionIdentifier> active;
    m_tracker.ReadEachConnection(
        [&active] (auto const& id, auto&) -> CallbackIteration {
            active.push_back(id);
            return CallbackIteration::Continue;
        },
        UpdateTimepointFilter::MatchPredicate,
        [&timepoint] (TimeUtils::Timepoint const& updated) -> bool { return (updated == timepoint); });

    EXPECT_TRUE(std::ranges::equal(active, expectedActive));

    std::vector<std::string> inactive;
    m_tracker.UpdateEachConnection(
        [&inactive] (auto const& id, auto&) -> CallbackIteration {
            inactive.push_back(id);
            return CallbackIteration::Continue;
        },
        UpdateTimepointFilter::MatchPredicate,
        [&timepoint] (TimeUtils::Timepoint const& updated) -> bool { return (updated < timepoint); });

    EXPECT_TRUE(std::ranges::equal(inactive, expectedInactive));
}

//----------------------------------------------------------------------------------------------------------------------