//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Processor/AuthorizedProcessor.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Security/CipherService.hpp"
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

class EndpointResources;
class EventObserver;

void SetupResources(EndpointResources& alpha, EndpointResources& omega);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

constexpr std::uint32_t Iterations = 10;
constexpr std::uint32_t MissedMessageLimit = 16;
constexpr std::string_view QueryRoute = "/query";
constexpr std::string_view QueryRequestData = "Query Request";
constexpr std::string_view QueryResponseData = "Query Response";

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::EndpointResources
{
public:
    EndpointResources(
        Node::SharedIdentifier const& spIdentifier,
        std::shared_ptr<Scheduler::Registrar> const& spRegistrar,
        std::shared_ptr<Event::Publisher> const& spEventPublisher,
        Configuration::Options::Endpoint const& options,
        std::optional<Network::RemoteAddress> const& optRemoteAddress = {});

    Network::Test::MessageProcessor& GetProcessor() const { return *m_upMessageProcessor; }
    Network::Test::SingleResolutionService& GetMediator() const { return *m_spMediator; }
    Network::TCP::Endpoint& GetEndpoint() const { return *m_upEndpoint; }

private:
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::unique_ptr<Network::Test::MessageProcessor> m_upMessageProcessor;
    std::shared_ptr<Network::Test::SingleResolutionService> m_spMediator;
    std::unique_ptr<Network::TCP::Endpoint> m_upEndpoint;
};

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
    
    auto const alphaOptions = Configuration::Options::Endpoint{ Network::Protocol::TCP, "lo", "*:35216" };
    auto const omegaOptions = Configuration::Options::Endpoint{ Network::Protocol::TCP, "lo", "*:35217" };

    auto const alphaAddress = Network::RemoteAddress{
        alphaOptions.GetProtocol(),
        alphaOptions.GetBinding().GetUri(),
        true,
        Network::RemoteAddress::Origin::User
    };
    
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spRegistrar);

    // Create the server resources. The peer mediator stub will store a single Peer::Proxy representing the client.
    local::EndpointResources alpha{ test::ServerIdentifier, spRegistrar, spEventPublisher, alphaOptions };
    EXPECT_EQ(alpha.GetEndpoint().GetProtocol(), Network::Protocol::TCP);
    ASSERT_EQ(alpha.GetEndpoint().GetBinding(), alphaOptions.GetBinding()); // The binding should be cached before start. 

    // Create the client resources. The peer mediator stub will store a single Peer::Proxy representing the server.
    local::EndpointResources omega{ test::ClientIdentifier, spRegistrar, spEventPublisher, omegaOptions, alphaAddress };
    EXPECT_EQ(omega.GetEndpoint().GetProtocol(), Network::Protocol::TCP);
    ASSERT_EQ(omega.GetEndpoint().GetBinding(), omegaOptions.GetBinding()); // The binding should be cached before start. 
    
    // Reset the heartbeat values for the next connect cycle. 
    local::SetupResources(alpha, omega);

    // Initialize the endpoint event tester before starting the endpoints. Otherwise, it's a race to subscribe to 
    // the emitted events before the threads can emit them. 
    local::EventObserver observer(spEventPublisher, { alpha.GetEndpoint().GetIdentifier(), omega.GetEndpoint().GetIdentifier() });
    ASSERT_TRUE(observer.SubscribedToAllAdvertisedEvents());
    spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    // Verify we can start a client before the server is up and that we can adjust the connection parameters 
    // to make the retry period reasonable for the purposes of testing. 
    omega.GetEndpoint().GetProperties().SetConnectionTimeout(125ms);
    omega.GetEndpoint().GetProperties().SetConnectionRetryLimit(5);
    omega.GetEndpoint().GetProperties().SetConnectionRetryInterval(25ms);
    omega.GetEndpoint().Startup();

    std::this_thread::sleep_for(100ms); // Sleep some period of time to verify retries. 
    alpha.GetEndpoint().Startup(); // Start the server endpoint before the client. 

    // Wait a period of time to ensure a connection between the server and client is initiated.
    std::this_thread::sleep_for(100ms);

    // Verify that the client endpoint used the connection declaration method on the mediator.
    EXPECT_TRUE(alpha.GetProcessor().ReceivedHeartbeatRequest());
    EXPECT_TRUE(omega.GetProcessor().ReceivedHeartbeatResponse());

    // Acquire the peer associated with the server endpoint from the perspective of the client.
    auto const spOmegaPeer = omega.GetMediator().GetPeer();
    ASSERT_TRUE(spOmegaPeer);

    // Acquire the peer associated with the server endpoint from the perspective of the client.
    auto spAlphaPeer = alpha.GetMediator().GetPeer();
    ASSERT_TRUE(spAlphaPeer);

    auto const PassMessages = [&] () {
        // Acquire the message context for the client peer's endpoint.
        auto const optClientContext = spOmegaPeer->GetMessageContext(omega.GetEndpoint().GetIdentifier());
        ASSERT_TRUE(optClientContext);

        // Build an application message to be sent to the server.
        auto const optQueryRequest = Message::Application::Parcel::GetBuilder()
            .SetContext(*optClientContext)
            .SetSource(*test::ClientIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(test::QueryRoute)
            .SetPayload({ test::QueryRequestData })
            .ValidatedBuild();
        ASSERT_TRUE(optQueryRequest);

        // Acquire the message context for the client peer's endpoint.
        auto const optServerContext = spAlphaPeer->GetMessageContext(alpha.GetEndpoint().GetIdentifier());
        ASSERT_TRUE(optServerContext);

        // Build an application message to be sent to the client.
        auto const optQueryResponse = Message::Application::Parcel::GetBuilder()
            .SetContext(*optServerContext)
            .SetSource(*test::ServerIdentifier)
            .SetDestination(*test::ClientIdentifier)
            .SetRoute(test::QueryRoute)
            .SetPayload({ test::QueryResponseData })
            .ValidatedBuild();
        ASSERT_TRUE(optQueryResponse);

        // Note: The requests and responses are typically moved during scheduling. We are going to create a new 
        // string to be moved each call instead of regenerating the packed content.  
        auto const spRequest = optQueryRequest->GetShareablePack();
        ASSERT_TRUE(spRequest && !spRequest->empty());

        auto const spResponse = optQueryResponse->GetShareablePack();
        ASSERT_TRUE(spResponse && !spResponse->empty());

        // Send the initial request to the server through the peer.
        EXPECT_TRUE(spOmegaPeer->ScheduleSend(optClientContext->GetEndpointIdentifier(), spRequest));

        // For some number of iterations enter request/response cycle using the peers obtained from the processors. 
        for (std::uint32_t iterations = 0; iterations < test::Iterations; ++iterations) {
            // Wait a period of time to ensure the request has been sent and received. 
            std::this_thread::sleep_for(1ms);

            // Handle the receipt of a request sent to the server.
            {
                std::uint32_t attempts = 0;
                std::optional<Message::Application::Parcel> optRequest;
                do {
                    optRequest = alpha.GetProcessor().GetNextMessage();
                    std::this_thread::sleep_for(1ms);
                    ++attempts;
                } while (!optRequest && attempts < test::MissedMessageLimit);
                ASSERT_TRUE(optRequest);

                // Verify the received request matches the one that was sent through the client.
                EXPECT_EQ(optRequest->GetHeader().GetVersion(), std::make_pair(0, 0));
                EXPECT_EQ(optRequest->GetHeader().GetMessageProtocol(), Message::Protocol::Application);
                EXPECT_EQ(optRequest->GetHeader().GetSource(), *test::ClientIdentifier);
                EXPECT_EQ(optRequest->GetHeader().GetDestinationType(), Message::Destination::Node);
                EXPECT_EQ(optRequest->GetHeader().GetDestination(), *test::ServerIdentifier);

                EXPECT_EQ(optRequest->GetRoute(), test::QueryRoute);
                EXPECT_EQ(optRequest->GetPayload().GetStringView(), test::QueryRequestData);

                // Send a response to the client
                if (auto const spRequestPeer = optRequest->GetContext().GetProxy().lock(); spRequestPeer) {
                    EXPECT_TRUE(spRequestPeer->ScheduleSend(optRequest->GetContext().GetEndpointIdentifier(), spResponse));
                }
            }

            // Wait a period of time to ensure the response has been sent and received. 
            std::this_thread::sleep_for(1ms);

            // Handle the receipt of a response sent to the client.
            {
                std::uint32_t attempts = 0;
                std::optional<Message::Application::Parcel> optResponse;
                do {
                    optResponse = omega.GetProcessor().GetNextMessage();
                    std::this_thread::sleep_for(1ms);
                    ++attempts;
                } while (!optResponse && attempts < test::MissedMessageLimit);
                ASSERT_TRUE(optResponse);

                // Verify the received response matches the one that was sent through the server.
                EXPECT_EQ(optResponse->GetHeader().GetVersion(), std::make_pair(0, 0));
                EXPECT_EQ(optResponse->GetHeader().GetMessageProtocol(), Message::Protocol::Application);
                EXPECT_EQ(optResponse->GetHeader().GetSource(), *test::ServerIdentifier);
                EXPECT_EQ(optResponse->GetHeader().GetDestinationType(), Message::Destination::Node);
                EXPECT_EQ(optResponse->GetHeader().GetDestination(), *test::ClientIdentifier);

                EXPECT_EQ(optResponse->GetRoute(), test::QueryRoute);
                EXPECT_EQ(optResponse->GetPayload().GetStringView(), test::QueryResponseData);

                // Send a request to the server.
                if (auto const spResponsePeer = optResponse->GetContext().GetProxy().lock(); spResponsePeer) {
                    EXPECT_TRUE(spResponsePeer->ScheduleSend(optResponse->GetContext().GetEndpointIdentifier(), spRequest));
                }
            }
        }
        
        std::this_thread::sleep_for(100ms); // Wait to ensure all messages have been processed. 
    };

    PassMessages(); // Verify we can pass messages using the TCP sockets. 

    EXPECT_TRUE(omega.GetEndpoint().ScheduleDisconnect(alphaAddress)); // Verify we can disconnect via the client.
    std::this_thread::sleep_for(100ms); // Wait to ensure the client picks up the command.

    // There should be one message left over after message loop, verify that we can still access the peer and read
    // the message when the mediator has kept the peer alive. 
    {
        auto const optDisconnectedRequest = alpha.GetProcessor().GetNextMessage();
        ASSERT_TRUE(optDisconnectedRequest);

        auto const spDisconnectedPeer = optDisconnectedRequest->GetContext().GetProxy().lock();
        ASSERT_TRUE(spDisconnectedPeer);
        EXPECT_EQ(spDisconnectedPeer, spAlphaPeer);
        EXPECT_EQ(spDisconnectedPeer->RegisteredEndpointCount(), 0);

        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetVersion(), std::make_pair(0, 0));
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetMessageProtocol(), Message::Protocol::Application);
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetSource(), *test::ClientIdentifier);
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetDestinationType(), Message::Destination::Node);
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetDestination(), *test::ServerIdentifier);

        EXPECT_EQ(optDisconnectedRequest->GetRoute(), test::QueryRoute);
        EXPECT_EQ(optDisconnectedRequest->GetPayload().GetStringView(), test::QueryRequestData);
    }

    // Reset the heartbeat values for the next connect cycle. 
    local::SetupResources(alpha, omega);

    EXPECT_TRUE(omega.GetEndpoint().ScheduleConnect(alphaAddress)); // Verify we can reconnect. 
    std::this_thread::sleep_for(100ms); // Wait to ensure the client  picks up the command. 

    // Verify a new set of heartbeats have been received after reconnecting. 
    EXPECT_TRUE(alpha.GetProcessor().ReceivedHeartbeatRequest());
    EXPECT_TRUE(omega.GetProcessor().ReceivedHeartbeatResponse());

    PassMessages(); // Verify we can pass messages using the TCP sockets after reconnecting. 

    // Verify we can disconnect through a peer via the registered disconnect scheduler.
    EXPECT_TRUE(spAlphaPeer->ScheduleDisconnect());
    std::this_thread::sleep_for(100ms); // Wait to ensure the server picks up the command.

    // Reset the heartbeat values for the next connect cycle. 
    local::SetupResources(alpha, omega);

    EXPECT_TRUE(omega.GetEndpoint().ScheduleConnect(alphaAddress)); // Verify we can reconnect. 
    std::this_thread::sleep_for(100ms); // Wait to ensure the client picks up the command. 

    // Verify a new set of heartbeats have been received after reconnecting. 
    EXPECT_TRUE(alpha.GetProcessor().ReceivedHeartbeatRequest());
    EXPECT_TRUE(omega.GetProcessor().ReceivedHeartbeatResponse());

    PassMessages(); // Verify we can pass messages using the TCP sockets after reconnecting. 

    // Shutdown the endpoints. Note: The endpoints destructor can handle the shutdown for us. However, we will 
    // need to test the state and events fired after shutdown. 
    EXPECT_TRUE(omega.GetEndpoint().Shutdown());
    EXPECT_TRUE(alpha.GetEndpoint().Shutdown());
    std::this_thread::sleep_for(100ms); // Wait to ensure the endpoints pick up the commands.

    // Verify that the last message for a completely removed peer is still accessible, but we can't pack messages for them. 
    {
        alpha.GetMediator().Reset();
        spAlphaPeer.reset();
        auto const optDisconnectedRequest = alpha.GetProcessor().GetNextMessage();
        ASSERT_TRUE(optDisconnectedRequest);
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetVersion(), std::make_pair(0, 0));
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetMessageProtocol(), Message::Protocol::Application);
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetSource(), *test::ClientIdentifier);
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetDestinationType(), Message::Destination::Node);
        EXPECT_EQ(optDisconnectedRequest->GetHeader().GetDestination(), *test::ServerIdentifier);

        EXPECT_EQ(optDisconnectedRequest->GetRoute(), test::QueryRoute);
        EXPECT_EQ(optDisconnectedRequest->GetPayload().GetStringView(), test::QueryRequestData);

        EXPECT_TRUE(optDisconnectedRequest->GetPack().empty());
    }

    EXPECT_EQ(alpha.GetProcessor().InvalidMessageCount(), 0ul );
    EXPECT_EQ(omega.GetProcessor().InvalidMessageCount(), 0ul );
    EXPECT_TRUE(observer.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

local::EndpointResources::EndpointResources(
    Node::SharedIdentifier const& spIdentifier,
    std::shared_ptr<Scheduler::Registrar> const& spRegistrar,
    std::shared_ptr<Event::Publisher> const& spEventPublisher,
    Configuration::Options::Endpoint const& options,
    std::optional<Network::RemoteAddress> const& optRemoteAddress)
    : m_spServiceProvider(std::make_shared<Node::ServiceProvider>())
    , m_upMessageProcessor(std::make_unique<Network::Test::MessageProcessor>(spIdentifier))
    , m_spMediator(std::make_shared<Network::Test::SingleResolutionService>(spIdentifier, m_upMessageProcessor.get(), m_spServiceProvider))
{
    m_spServiceProvider->Register<IResolutionService>(m_spMediator);

    Network::Endpoint::Properties const properties{ options };
    m_upEndpoint = std::make_unique<Network::TCP::Endpoint>(properties);
    m_upEndpoint->Register(spEventPublisher);
    m_upEndpoint->Register(m_spMediator.get());
    
    if (!m_upEndpoint->ScheduleBind(properties.GetBinding())) {
        throw std::runtime_error("Unable to schedule a bind when setting up the test endpoint!");
    }

    if (optRemoteAddress && !m_upEndpoint->ScheduleConnect(*optRemoteAddress)) {
        throw std::runtime_error("Unable to schedule a connect when setting up the test endpoint!");
    }
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

void local::SetupResources(EndpointResources& alpha, EndpointResources& omega)
{
    auto&& [upInitiatorPackage, upAcceptorPackage] = Security::Test::GenerateCipherPackages();
    alpha.GetProcessor().Reset();
    alpha.GetMediator().SetCipherPackage(std::move(upInitiatorPackage));
    
    omega.GetProcessor().Reset();
    omega.GetMediator().SetCipherPackage(std::move(upAcceptorPackage));
}

//----------------------------------------------------------------------------------------------------------------------
