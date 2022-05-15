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

std::unique_ptr<Network::TCP::Endpoint> MakeServer(
    Event::SharedPublisher const& spEventPublisher, std::shared_ptr<IResolutionService> const& spResolutionService);

std::unique_ptr<Network::TCP::Endpoint> MakeClient(
    Event::SharedPublisher const& spEventPublisher,
    std::shared_ptr<IResolutionService> const& spResolutionService,
    Network::RemoteAddress const& address);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

auto Options = Configuration::Options::Endpoint{ Network::Protocol::TCP, "lo", "*:35216" };

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
    ASSERT_TRUE(test::Options.Initialize(Network::Test::RuntimeOptions, spdlog::get(Logger::Name::Core.data())));

    auto const ServerAddress = Network::RemoteAddress{
        test::Options.GetProtocol(), test::Options.GetBinding().GetUri(), true, Network::RemoteAddress::Origin::User };
    
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spRegistrar);

    // Create the server resources. The peer mediator stub will store a single Peer::Proxy representing the client.
    auto const spServerProvider = std::make_shared<Node::ServiceProvider>();
    auto const upServerProcessor = std::make_unique<Network::Test::MessageProcessor>(test::ServerIdentifier);
    auto const spServerMediator = std::make_shared<Network::Test::SingleResolutionService>(
        test::ServerIdentifier, upServerProcessor.get(), spServerProvider);
    spServerProvider->Register<IResolutionService>(spServerMediator);
    auto upServerEndpoint = local::MakeServer(spEventPublisher, spServerMediator);
    EXPECT_EQ(upServerEndpoint->GetProtocol(), Network::Protocol::TCP);
    EXPECT_EQ(upServerEndpoint->GetProperties().GetOperation(), Network::Operation::Server);
    ASSERT_EQ(upServerEndpoint->GetBinding(), test::Options.GetBinding()); // The binding should be cached before start. 

    // Create the client resources. The peer mediator stub will store a single Peer::Proxy representing the server.
    auto const spClientProvider = std::make_shared<Node::ServiceProvider>();
    auto const upClientProcessor = std::make_unique<Network::Test::MessageProcessor>(test::ClientIdentifier);
    auto const spClientMediator = std::make_shared<Network::Test::SingleResolutionService>(
        test::ClientIdentifier, upClientProcessor.get(), spClientProvider);
    spClientProvider->Register<IResolutionService>(spClientMediator);
    auto upClientEndpoint = local::MakeClient(spEventPublisher, spClientMediator, ServerAddress);
    EXPECT_EQ(upClientEndpoint->GetProtocol(), Network::Protocol::TCP);
    EXPECT_EQ(upClientEndpoint->GetProperties().GetOperation(), Network::Operation::Client);

    // Initialize the endpoint event tester before starting the endpoints. Otherwise, it's a race to subscribe to 
    // the emitted events before the threads can emit them. 
    local::EventObserver observer(
        spEventPublisher, { upServerEndpoint->GetIdentifier(), upClientEndpoint->GetIdentifier() });
    ASSERT_TRUE(observer.SubscribedToAllAdvertisedEvents());
    spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    // Verify we can start a client before the server is up and that we can adjust the connection parameters 
    // to make the retry period reasonable for the purposes of testing. 
    upClientEndpoint->GetProperties().SetConnectionTimeout(125ms);
    upClientEndpoint->GetProperties().SetConnectionRetryLimit(5);
    upClientEndpoint->GetProperties().SetConnectionRetryInterval(25ms);
    upClientEndpoint->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep some peroid of time to verify retries. 
    upServerEndpoint->Startup(); // Start the server endpoint before the client. 

    // Wait a period of time to ensure a connection between the server and client is initiated.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify that the client endpoint used the connection declaration method on the mediator.
    EXPECT_TRUE(upServerProcessor->ReceviedHeartbeatRequest());
    EXPECT_TRUE(upClientProcessor->ReceviedHeartbeatResponse());

    // Acquire the peer associated with the server endpoint from the perspective of the client.
    auto const spClientPeer = spClientMediator->GetPeer();
    ASSERT_TRUE(spClientPeer);

    // Acqure the message context for the client peer's endpoint.
    auto const optClientContext = spClientPeer->GetMessageContext(upClientEndpoint->GetIdentifier());
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
    auto spServerPeer = spServerMediator->GetPeer();
    ASSERT_TRUE(spServerPeer);

    // Acqure the message context for the client peer's endpoint.
    auto const optServerContext = spServerPeer->GetMessageContext(upServerEndpoint->GetIdentifier());
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
        EXPECT_TRUE(spClientPeer->ScheduleSend(optClientContext->GetEndpointIdentifier(), spRequest));

        // For some number of iterations enter request/response cycle using the peers obtained from the processors. 
        for (std::uint32_t iterations = 0; iterations < test::Iterations; ++iterations) {
            // Wait a period of time to ensure the request has been sent and received. 
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            // Handle the reciept of a request sent to the server.
            {
                std::uint32_t attempts = 0;
                std::optional<Message::Application::Parcel> optRequest;
                do {
                    optRequest = upServerProcessor->GetNextMessage();
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

            // Handle the reciept of a response sent to the client.
            {
                std::uint32_t attempts = 0;
                std::optional<Message::Application::Parcel> optResponse;
                do {
                    optResponse = upClientProcessor->GetNextMessage();
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

    EXPECT_TRUE(upClientEndpoint->ScheduleDisconnect(ServerAddress)); // Verify we can disconnect via the client.
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the client picks up the command.

    // There should be one message left over after message loop, verify that we can still access the peer and read
    // the message when the mediator has kept the peer alive. 
    {
        auto const optDisconnectedRequest = upServerProcessor->GetNextMessage();
        ASSERT_TRUE(optDisconnectedRequest);

        auto const spDisconnectedPeer = optDisconnectedRequest->GetContext().GetProxy().lock();
        ASSERT_TRUE(spDisconnectedPeer);
        EXPECT_EQ(spDisconnectedPeer, spServerPeer);
        EXPECT_EQ(spDisconnectedPeer->RegisteredEndpointCount(), 0);
        EXPECT_EQ(optDisconnectedRequest->GetPack(), *spRequest);
    }

    // Reset the heartbeat values for the next connect cycle. 
    upServerProcessor->Reset(); 
    upClientProcessor->Reset();

    EXPECT_TRUE(upClientEndpoint->ScheduleConnect(ServerAddress)); // Verify we can reconnect. 
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the client  picks up the command. 

    // Verify a new set of heartbeats have been received after reconnecting. 
    EXPECT_TRUE(upServerProcessor->ReceviedHeartbeatRequest());
    EXPECT_TRUE(upClientProcessor->ReceviedHeartbeatResponse());

    PassMessages(); // Verify we can pass messages using the TCP sockets after reconnecting. 

    // Verify we can disconnect through a peer via the registered disconnect scheduler.
    EXPECT_TRUE(spServerPeer->ScheduleDisconnect());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the server picks up the command.

    // Reset the heartbeat values for the next connect cycle. 
    upServerProcessor->Reset(); 
    upClientProcessor->Reset();

    EXPECT_TRUE(upClientEndpoint->ScheduleConnect(ServerAddress)); // Verify we can reconnect. 
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the client picks up the command. 

    // Verify a new set of heartbeats have been received after reconnecting. 
    EXPECT_TRUE(upServerProcessor->ReceviedHeartbeatRequest());
    EXPECT_TRUE(upClientProcessor->ReceviedHeartbeatResponse());

    PassMessages(); // Verify we can pass messages using the TCP sockets after reconnecting. 

    // Shutdown the endpoints. Note: The endpoints destructor can handle the shutdown for us. However, we will 
    // need to test the state and events fired after shutdown. 
    EXPECT_TRUE(upClientEndpoint->Shutdown());
    EXPECT_TRUE(upServerEndpoint->Shutdown());
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Wait to ensure the endpoints pick up the commands.

    // Verify that we cannot unpack messages for peers that have been completely removed. 
    {
        spServerMediator->Reset();
        spServerPeer.reset();
        auto const optDisconnectedRequest = upServerProcessor->GetNextMessage();
        ASSERT_TRUE(optDisconnectedRequest);
        EXPECT_NE(optDisconnectedRequest->GetPack(), *spRequest);
    }

    EXPECT_EQ(upServerProcessor->InvalidMessageCount(), std::uint32_t(0));
    EXPECT_EQ(upClientProcessor->InvalidMessageCount(), std::uint32_t(0));
    EXPECT_TRUE(observer.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Network::TCP::Endpoint> local::MakeServer(
    Event::SharedPublisher const& spEventPublisher, std::shared_ptr<IResolutionService> const& spResolutionService)
{
    auto const properties = Network::Endpoint::Properties{ Network::Operation::Server, test::Options };
    auto upServerEndpoint = std::make_unique<Network::TCP::Endpoint>(properties);
    upServerEndpoint->Register(spEventPublisher);
    upServerEndpoint->Register(spResolutionService.get());
    bool const result = upServerEndpoint->ScheduleBind(test::Options.GetBinding());
    return (result) ? std::move(upServerEndpoint) : nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Network::TCP::Endpoint> local::MakeClient(
    Event::SharedPublisher const& spEventPublisher,
    std::shared_ptr<IResolutionService> const& spResolutionService,
    Network::RemoteAddress const& address)
{
    auto const properties = Network::Endpoint::Properties{ Network::Operation::Client, test::Options };
    auto upClientEndpoint = std::make_unique<Network::TCP::Endpoint>(properties);
    upClientEndpoint->Register(spEventPublisher);
    upClientEndpoint->Register(spResolutionService.get());
    bool const result = upClientEndpoint->ScheduleConnect(address);
    return (result) ? std::move(upClientEndpoint) : nullptr;
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
        [&] (Endpoint::Identifier identifier, Protocol protocol, Operation operation) {
            if (protocol == Protocol::Invalid || operation == Operation::Invalid) { return; }
            if (auto const itr = m_tracker.find(identifier); itr != m_tracker.end()) {
                itr->second.emplace_back(Event::Type::EndpointStarted);
            }
        });

    spPublisher->Subscribe<Event::Type::EndpointStopped>(
        [&] (Endpoint::Identifier identifier, Protocol protocol, Operation operation, StopCause cause) {
            if (protocol == Protocol::Invalid || operation == Operation::Invalid) { return; }
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
