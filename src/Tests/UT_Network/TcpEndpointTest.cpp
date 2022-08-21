//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class EventObserver;

std::unique_ptr<Network::TCP::Endpoint> MakeEndpoint(
    Configuration::Options::Endpoint const& options,
    Event::SharedPublisher const& spEventPublisher,
    std::shared_ptr<IResolutionService> const& spResolutionService,
    std::optional<Network::RemoteAddress> const& optRemoteAddress = {});

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

auto AlphaOptions = Configuration::Options::Endpoint{ Network::Protocol::TCP, "lo", "*:35216" };
auto OmegaOptions = Configuration::Options::Endpoint{ Network::Protocol::TCP, "lo", "*:35217" };

constexpr std::uint32_t Iterations = 10;
constexpr std::uint32_t MissedMessageLimit = 16;
constexpr std::string_view QueryRoute = "/query";

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::EventObserver
{
public:
    using EndpointIdentifiers = std::vector<Network::Endpoint::Identifier>;
    using EventRecord = std::vector<Event::Type>;
    using EventTracker = std::unordered_map<Network::Endpoint::Identifier, EventRecord>;

    EventObserver(Event::SharedPublisher const& spPublisher, EndpointIdentifiers const& identifiers);
    bool SubscribedToAllAdvertisedEvents() const;
    bool ReceivedExpectedEventSequence() const;

private:
    static constexpr std::uint32_t ExpectedEventCount = 2; // The number of events each endpoint should fire. 
    Event::SharedPublisher m_spPublisher;
    EventTracker m_tracker;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(TcpEndpointSuite, SingleConnectionTest)
{
    using namespace std::chrono_literals;
    ASSERT_TRUE(test::AlphaOptions.Initialize(Network::Test::RuntimeOptions, spdlog::get(Logger::Name::Core.data())));
    ASSERT_TRUE(test::OmegaOptions.Initialize(Network::Test::RuntimeOptions, spdlog::get(Logger::Name::Core.data())));

    auto const alphaAddress = Network::RemoteAddress{
        test::AlphaOptions.GetProtocol(),
        test::AlphaOptions.GetBinding().GetUri(),
        true,
        Network::RemoteAddress::Origin::User
    };
    
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spRegistrar);

    // Create the server resources. The peer mediator stub will store a single Peer::Proxy representing the client.
    auto const spAlphaProvider = std::make_shared<Node::ServiceProvider>();
    auto const upAlphaProcessor = std::make_unique<Network::Test::MessageProcessor>(test::ServerIdentifier);
    auto const spAlphaMediator = std::make_shared<Network::Test::SingleResolutionService>(
        test::ServerIdentifier, upAlphaProcessor.get(), spAlphaProvider);
    spAlphaProvider->Register<IResolutionService>(spAlphaMediator);
    auto upAlphaEndpoint = local::MakeEndpoint(test::AlphaOptions, spEventPublisher, spAlphaMediator);
    EXPECT_EQ(upAlphaEndpoint->GetProtocol(), Network::Protocol::TCP);
    ASSERT_EQ(upAlphaEndpoint->GetBinding(), test::AlphaOptions.GetBinding()); // The binding should be cached before start. 

    // Create the client resources. The peer mediator stub will store a single Peer::Proxy representing the server.
    auto const spOmegaProvider = std::make_shared<Node::ServiceProvider>();
    auto const upOmegaProcessor = std::make_unique<Network::Test::MessageProcessor>(test::ClientIdentifier);
    auto const spOmegaMediator = std::make_shared<Network::Test::SingleResolutionService>(
        test::ClientIdentifier, upOmegaProcessor.get(), spOmegaProvider);
    spOmegaProvider->Register<IResolutionService>(spOmegaMediator);
    auto upOmegaEndpoint = local::MakeEndpoint(test::OmegaOptions, spEventPublisher, spOmegaMediator, alphaAddress);
    EXPECT_EQ(upOmegaEndpoint->GetProtocol(), Network::Protocol::TCP);
    ASSERT_EQ(upOmegaEndpoint->GetBinding(), test::OmegaOptions.GetBinding()); // The binding should be cached before start. 

    // Initialize the endpoint event tester before starting the endpoints. Otherwise, it's a race to subscribe to 
    // the emitted events before the threads can emit them. 
    local::EventObserver observer(
        spEventPublisher, { upAlphaEndpoint->GetIdentifier(), upOmegaEndpoint->GetIdentifier() });
    ASSERT_TRUE(observer.SubscribedToAllAdvertisedEvents());
    spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    // Verify we can start a client before the server is up and that we can adjust the connection parameters 
    // to make the retry period reasonable for the purposes of testing. 
    upOmegaEndpoint->GetProperties().SetConnectionTimeout(125ms);
    upOmegaEndpoint->GetProperties().SetConnectionRetryLimit(5);
    upOmegaEndpoint->GetProperties().SetConnectionRetryInterval(25ms);
    upOmegaEndpoint->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep some period of time to verify retries. 
    upAlphaEndpoint->Startup(); // Start the server endpoint before the client. 

    // Wait a period of time to ensure a connection between the server and client is initiated.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify that the client endpoint used the connection declaration method on the mediator.
    EXPECT_TRUE(upAlphaProcessor->ReceivedHeartbeatRequest());
    EXPECT_TRUE(upOmegaProcessor->ReceivedHeartbeatResponse());

    // Acquire the peer associated with the server endpoint from the perspective of the client.
    auto const spOmegaPeer = spOmegaMediator->GetPeer();
    ASSERT_TRUE(spOmegaPeer);

    // Acquire the message context for the client peer's endpoint.
    auto const optClientContext = spOmegaPeer->GetMessageContext(upOmegaEndpoint->GetIdentifier());
    ASSERT_TRUE(optClientContext);

    // Build an application message to be sent to the server.
    auto const optQueryRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(*optClientContext)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(test::QueryRoute)
        .SetPayload({ "Query Request" })
        .ValidatedBuild();
    ASSERT_TRUE(optQueryRequest);

    // Acquire the peer associated with the server endpoint from the perspective of the client.
    auto spAlphaPeer = spAlphaMediator->GetPeer();
    ASSERT_TRUE(spAlphaPeer);

    // Acquire the message context for the client peer's endpoint.
    auto const optServerContext = spAlphaPeer->GetMessageContext(upAlphaEndpoint->GetIdentifier());
    ASSERT_TRUE(optServerContext);

    // Build an application message to be sent to the client.
    auto const optQueryResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(*optServerContext)
        .SetSource(*test::ServerIdentifier)
        .SetDestination(*test::ClientIdentifier)
        .SetRoute(test::QueryRoute)
        .SetPayload({ "Query Response" })
        .ValidatedBuild();
    ASSERT_TRUE(optQueryResponse);

    // Note: The requests and responses are typically moved during scheduling. We are going to create a new 
    // string to be moved each call instead of regenerating the packed content.  
    auto const spRequest = optQueryRequest->GetShareablePack();
    ASSERT_TRUE(spRequest && !spRequest->empty());

    auto const spResponse = optQueryResponse->GetShareablePack();
    ASSERT_TRUE(spResponse && !spResponse->empty());

    auto const PassMessages = [&] () {
        // Send the initial request to the server through the peer.
        EXPECT_TRUE(spOmegaPeer->ScheduleSend(optClientContext->GetEndpointIdentifier(), spRequest));

        // For some number of iterations enter request/response cycle using the peers obtained from the processors. 
        for (std::uint32_t iterations = 0; iterations < test::Iterations; ++iterations) {
            // Wait a period of time to ensure the request has been sent and received. 
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Handle the receipt of a request sent to the server.
            {
                std::uint32_t attempts = 0;
                std::optional<Message::Application::Parcel> optRequest;
                do {
                    optRequest = upAlphaProcessor->GetNextMessage();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    ++attempts;
                } while (!optRequest && attempts < test::MissedMessageLimit);
                ASSERT_TRUE(optRequest);

                // Verify the received request matches the one that was sent through the client.
                EXPECT_EQ(optRequest->GetPack(), *spRequest);

                // Send a response to the client
                if (auto const spRequestPeer = optRequest->GetContext().GetProxy().lock(); spRequestPeer) {
                    EXPECT_TRUE(spRequestPeer->ScheduleSend(optRequest->GetContext().GetEndpointIdentifier(), spResponse));
                }
            }

            // Wait a period of time to ensure the response has been sent and received. 
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Handle the receipt of a response sent to the client.
            {
                std::uint32_t attempts = 0;
                std::optional<Message::Application::Parcel> optResponse;
                do {
                    optResponse = upOmegaProcessor->GetNextMessage();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    ++attempts;
                } while (!optResponse && attempts < test::MissedMessageLimit);
                ASSERT_TRUE(optResponse);

                // Verify the received response matches the one that was sent through the server.
                EXPECT_EQ(optResponse->GetPack(), *spResponse);

                // Send a request to the server.
                if (auto const spResponsePeer = optResponse->GetContext().GetProxy().lock(); spResponsePeer) {
                    EXPECT_TRUE(spResponsePeer->ScheduleSend(optResponse->GetContext().GetEndpointIdentifier(), spRequest));
                }
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure all messages have been processed. 
    };

    PassMessages(); // Verify we can pass messages using the TCP sockets. 

    EXPECT_TRUE(upOmegaEndpoint->ScheduleDisconnect(alphaAddress)); // Verify we can disconnect via the client.
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the client picks up the command.

    // There should be one message left over after message loop, verify that we can still access the peer and read
    // the message when the mediator has kept the peer alive. 
    {
        auto const optDisconnectedRequest = upAlphaProcessor->GetNextMessage();
        ASSERT_TRUE(optDisconnectedRequest);

        auto const spDisconnectedPeer = optDisconnectedRequest->GetContext().GetProxy().lock();
        ASSERT_TRUE(spDisconnectedPeer);
        EXPECT_EQ(spDisconnectedPeer, spAlphaPeer);
        EXPECT_EQ(spDisconnectedPeer->RegisteredEndpointCount(), 0);
        EXPECT_EQ(optDisconnectedRequest->GetPack(), *spRequest);
    }

    // Reset the heartbeat values for the next connect cycle. 
    upAlphaProcessor->Reset(); 
    upOmegaProcessor->Reset();

    EXPECT_TRUE(upOmegaEndpoint->ScheduleConnect(alphaAddress)); // Verify we can reconnect. 
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the client  picks up the command. 

    // Verify a new set of heartbeats have been received after reconnecting. 
    EXPECT_TRUE(upAlphaProcessor->ReceivedHeartbeatRequest());
    EXPECT_TRUE(upOmegaProcessor->ReceivedHeartbeatResponse());

    PassMessages(); // Verify we can pass messages using the TCP sockets after reconnecting. 

    // Verify we can disconnect through a peer via the registered disconnect scheduler.
    EXPECT_TRUE(spAlphaPeer->ScheduleDisconnect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the server picks up the command.

    // Reset the heartbeat values for the next connect cycle. 
    upAlphaProcessor->Reset(); 
    upOmegaProcessor->Reset();

    EXPECT_TRUE(upOmegaEndpoint->ScheduleConnect(alphaAddress)); // Verify we can reconnect. 
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the client picks up the command. 

    // Verify a new set of heartbeats have been received after reconnecting. 
    EXPECT_TRUE(upAlphaProcessor->ReceivedHeartbeatRequest());
    EXPECT_TRUE(upOmegaProcessor->ReceivedHeartbeatResponse());

    PassMessages(); // Verify we can pass messages using the TCP sockets after reconnecting. 

    // Shutdown the endpoints. Note: The endpoints destructor can handle the shutdown for us. However, we will 
    // need to test the state and events fired after shutdown. 
    EXPECT_TRUE(upOmegaEndpoint->Shutdown());
    EXPECT_TRUE(upAlphaEndpoint->Shutdown());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the endpoints pick up the commands.

    // Verify that we cannot unpack messages for peers that have been completely removed. 
    {
        spAlphaMediator->Reset();
        spAlphaPeer.reset();
        auto const optDisconnectedRequest = upAlphaProcessor->GetNextMessage();
        ASSERT_TRUE(optDisconnectedRequest);
        EXPECT_NE(optDisconnectedRequest->GetPack(), *spRequest);
    }

    EXPECT_EQ(upAlphaProcessor->InvalidMessageCount(), std::uint32_t(0));
    EXPECT_EQ(upOmegaProcessor->InvalidMessageCount(), std::uint32_t(0));
    EXPECT_TRUE(observer.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Network::TCP::Endpoint> local::MakeEndpoint(
    Configuration::Options::Endpoint const& options,
    Event::SharedPublisher const& spEventPublisher,
    std::shared_ptr<IResolutionService> const& spResolutionService,
    std::optional<Network::RemoteAddress> const& optRemoteAddress)
{
    Network::Endpoint::Properties const properties{ options };
    auto upEndpoint = std::make_unique<Network::TCP::Endpoint>(properties);
    upEndpoint->Register(spEventPublisher);
    upEndpoint->Register(spResolutionService.get());
    
    if (!upEndpoint->ScheduleBind(properties.GetBinding())) { return nullptr; }

    if (optRemoteAddress) {
        if(!upEndpoint->ScheduleConnect(*optRemoteAddress)) { return nullptr; }
    }

    return upEndpoint;
}

//----------------------------------------------------------------------------------------------------------------------

local::EventObserver::EventObserver(
    Event::SharedPublisher const& spPublisher, EndpointIdentifiers const& identifiers)
    : m_spPublisher(spPublisher)
    , m_tracker()
{
    using namespace Network;
    using StopCause = Event::Message<Event::Type::EndpointStopped>::Cause;
    using BindingFailure = Event::Message<Event::Type::BindingFailed>::Cause;
    using ConnectionFailure = Event::Message<Event::Type::ConnectionFailed>::Cause;

    // Convert the provided identifiers to an event record. 
    std::transform(
        identifiers.begin(), identifiers.end(), std::inserter(m_tracker, m_tracker.end()),
        [] (auto identifier) { return std::make_pair(identifier, EventRecord{}); });

    // Subscribe to all events fired by an endpoint. Each listener should only record valid events. 
    spPublisher->Subscribe<Event::Type::EndpointStarted>(
        [&] (Endpoint::Identifier identifier, BindingAddress const& binding) {
            if (!binding.IsValid()) { return; }
            if (auto const itr = m_tracker.find(identifier); itr != m_tracker.end()) {
                itr->second.emplace_back(Event::Type::EndpointStarted);
            }
        });

    spPublisher->Subscribe<Event::Type::EndpointStopped>(
        [&] (Endpoint::Identifier identifier, BindingAddress const& binding, StopCause cause) {
            if (!binding.IsValid()) { return; }
            if (cause != StopCause::ShutdownRequest) { return; }
            if (auto const itr = m_tracker.find(identifier); itr != m_tracker.end()) {
                itr->second.emplace_back(Event::Type::EndpointStopped);
            }
        });

    spPublisher->Subscribe<Event::Type::BindingFailed>(
        [&] (Endpoint::Identifier identifier, Network::BindingAddress const&, BindingFailure) {
            if (auto const itr = m_tracker.find(identifier); itr != m_tracker.end()) {
                itr->second.emplace_back(Event::Type::BindingFailed);
            }
        });

    spPublisher->Subscribe<Event::Type::ConnectionFailed>(
        [&] (Endpoint::Identifier identifier, Network::RemoteAddress const&, ConnectionFailure) {
            if (auto const itr = m_tracker.find(identifier); itr != m_tracker.end()) {
                itr->second.emplace_back(Event::Type::ConnectionFailed);
            }
        });
}

//----------------------------------------------------------------------------------------------------------------------

bool local::EventObserver::SubscribedToAllAdvertisedEvents() const
{
    // We expect to be subscribed to all events advertised by an endpoint. A failure here is most likely caused
    // by this test fixture being outdated. 
    return m_spPublisher->ListenerCount() == m_spPublisher->AdvertisedCount();
}

//----------------------------------------------------------------------------------------------------------------------

bool local::EventObserver::ReceivedExpectedEventSequence() const
{
    if (m_spPublisher->Dispatch() == 0) { return false; } // We expect that events have been published. 

    // Count the number of endpoints that match the expected number and sequence of events (e.g. start becomes stop). 
    std::size_t const count = std::ranges::count_if(m_tracker | std::views::values,
        [] (EventRecord const& record) -> bool
        {
            if (record.size() != ExpectedEventCount) { return false; }
            if (record[0] != Event::Type::EndpointStarted) { return false; }
            if (record[1] != Event::Type::EndpointStopped) { return false; }
            return true;
        });

    // We expect that all endpoints tracked meet the event sequence expectations. 
    if (count != m_tracker.size()) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
