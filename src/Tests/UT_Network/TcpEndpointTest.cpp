//----------------------------------------------------------------------------------------------------------------------
#include "MessageSinkStub.hpp"
#include "SinglePeerMediatorStub.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Handler/HandlerDefinitions.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
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

std::unique_ptr<Network::TCP::Endpoint> MakeTcpServer(
    Event::SharedPublisher const& spEventPublisher, std::shared_ptr<IPeerMediator> const& spPeerMediator);
std::unique_ptr<Network::TCP::Endpoint> MakeTcpClient(
    Event::SharedPublisher const& spEventPublisher, std::shared_ptr<IPeerMediator> const& spPeerMediator);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

constexpr Network::Protocol ProtocolType = Network::Protocol::TCP;
Network::BindingAddress ServerBinding(ProtocolType, "*:35216", "lo");

constexpr std::uint32_t Iterations = 1000;

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
    constexpr static std::uint32_t ExpectedEventCount = 2; // The number of events each endpoint should fire. 
    Event::SharedPublisher m_spPublisher;
    EventTracker m_tracker;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(TcpEndpointSuite, SingleConnectionTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    auto const spEventPublisher = std::make_shared<Event::Publisher>(spRegistrar);

    // Create the server resources. The peer mediator stub will store a single Peer::Proxy representing the client.
    auto upServerProcessor = std::make_unique<MessageSinkStub>(test::ServerIdentifier);
    auto const spServerMediator = std::make_shared<SinglePeerMediatorStub>(
        test::ServerIdentifier, upServerProcessor.get());
    auto upServerEndpoint = local::MakeTcpServer(spEventPublisher, spServerMediator);
    EXPECT_EQ(upServerEndpoint->GetProtocol(), Network::Protocol::TCP);
    EXPECT_EQ(upServerEndpoint->GetOperation(), Network::Operation::Server);
    ASSERT_EQ(upServerEndpoint->GetBinding(), test::ServerBinding); // The binding should be cached before start. 

    // Create the client resources. The peer mediator stub will store a single Peer::Proxy representing the server.
    auto upClientProcessor = std::make_unique<MessageSinkStub>(test::ClientIdentifier);
    auto const spClientMediator = std::make_shared<SinglePeerMediatorStub>(
        test::ClientIdentifier, upClientProcessor.get());
    auto upClientEndpoint = local::MakeTcpClient(spEventPublisher, spClientMediator);
    EXPECT_EQ(upClientEndpoint->GetProtocol(), Network::Protocol::TCP);
    EXPECT_EQ(upClientEndpoint->GetOperation(), Network::Operation::Client);

    // Initialize the endpoint event tester before starting the endpoints. Otherwise, it's a race to subscribe to 
    // the emitted events before the threads can emit them. 
    local::EventObserver observer(
        spEventPublisher, { upServerEndpoint->GetIdentifier(), upClientEndpoint->GetIdentifier() });
    ASSERT_TRUE(observer.SubscribedToAllAdvertisedEvents());
    spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.

    upServerEndpoint->Startup(); // Start the server endpoint before the client. 

    // Wait a period of time before starting the client. Otherwise, the client may start attempting a connection
    // before the server's listener is established. The client will retry on failure, but for the purposes
    // of this test we expect the connection to work on the first try (this avoids extended sleeps in the test).
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    upClientEndpoint->Startup();

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
    auto const optQueryRequest = ApplicationMessage::Builder()
        .SetMessageContext(*optClientContext)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Handler::Type::Query, 0)
        .SetPayload("Query Request")
        .ValidatedBuild();
    ASSERT_TRUE(optQueryRequest);

    // Acquire the peer associated with the server endpoint from the perspective of the client.
    auto const spServerPeer = spServerMediator->GetPeer();
    ASSERT_TRUE(spServerPeer);

    // Acqure the message context for the client peer's endpoint.
    auto const optServerContext = spServerPeer->GetMessageContext(upServerEndpoint->GetIdentifier());
    ASSERT_TRUE(optServerContext);

    // Build an application message to be sent to the client.
    auto const optQueryResponse = ApplicationMessage::Builder()
        .SetMessageContext(*optServerContext)
        .SetSource(*test::ServerIdentifier)
        .SetDestination(*test::ClientIdentifier)
        .SetCommand(Handler::Type::Query, 1)
        .SetPayload("Query Response")
        .ValidatedBuild();
    ASSERT_TRUE(optQueryResponse);

    // Note: The requests and responses are typically moved during scheduling. We are going to create a new 
    // string to be moved each call instead of regenerating the packed content.  
    auto const spRequest = optQueryRequest->GetShareablePack();
    auto const spResponse = optQueryResponse->GetShareablePack();

    // Send the initial request to the server through the peer.
    EXPECT_TRUE(spClientPeer->ScheduleSend(optClientContext->GetEndpointIdentifier(), spRequest));

    // For some number of iterations enter request/response cycle using the peers obtained
    // from the processors. 
    for (std::uint32_t iterations = 0; iterations < test::Iterations; ++iterations) {
        // Wait a period of time to ensure the request has been sent and received. 
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Handle the reciept of a request sent to the server.
        {
            std::optional<AssociatedMessage> optAssociatedRequest;
            do {
                optAssociatedRequest = upServerProcessor->GetNextMessage();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (!optAssociatedRequest);
            ASSERT_TRUE(optAssociatedRequest);

            // Verify the received request matches the one that was sent through the client.
            auto const& [wpRequestPeer, message] = *optAssociatedRequest;
            EXPECT_EQ(message.GetPack(), *spRequest);

            // Send a response to the client
            if (auto const spRequestPeer = wpRequestPeer.lock(); spRequestPeer) {
                EXPECT_TRUE(spRequestPeer->ScheduleSend(message.GetContext().GetEndpointIdentifier(), spResponse));
            }
        }

        // Wait a period of time to ensure the response has been sent and received. 
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

        // Handle the reciept of a response sent to the client.
        {
            std::optional<AssociatedMessage> optAssociatedResponse;
            do {
                optAssociatedResponse = upClientProcessor->GetNextMessage();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } while (!optAssociatedResponse);
            ASSERT_TRUE(optAssociatedResponse);

            // Verify the received response matches the one that was sent through the server.
            auto const& [wpResponsePeer, message] = *optAssociatedResponse;
            EXPECT_EQ(message.GetPack(), *spResponse);

            // Send a request to the server.
            if (auto const spResponsePeer = wpResponsePeer.lock(); spResponsePeer) {
                EXPECT_TRUE(spResponsePeer->ScheduleSend(message.GetContext().GetEndpointIdentifier(), spRequest));
            }
        }
    }

    // Shutdown the endpoints. Note: The endpoints destructor can handle the shutdown for us. However, we will 
    // need to test the state and events fired after shutdown. 
    EXPECT_TRUE(upClientEndpoint->Shutdown());
    EXPECT_TRUE(upServerEndpoint->Shutdown());

    EXPECT_EQ(upServerProcessor->InvalidMessageCount(), std::uint32_t(0));
    EXPECT_EQ(upClientProcessor->InvalidMessageCount(), std::uint32_t(0));
    EXPECT_TRUE(observer.ReceivedExpectedEventSequence());
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Network::TCP::Endpoint> local::MakeTcpServer(
    Event::SharedPublisher const& spEventPublisher, std::shared_ptr<IPeerMediator> const& spPeerMediator)
{
    auto upServerEndpoint = std::make_unique<Network::TCP::Endpoint>(Network::Operation::Server, spEventPublisher);
    upServerEndpoint->RegisterMediator(spPeerMediator.get());
    bool const result = upServerEndpoint->ScheduleBind(test::ServerBinding);
    return (result) ? std::move(upServerEndpoint) : nullptr;
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Network::TCP::Endpoint> local::MakeTcpClient(
    Event::SharedPublisher const& spEventPublisher, std::shared_ptr<IPeerMediator> const& spPeerMediator)
{
    using Origin = Network::RemoteAddress::Origin;
    auto upClientEndpoint = std::make_unique<Network::TCP::Endpoint>(Network::Operation::Client, spEventPublisher);
    Network::RemoteAddress address(test::ProtocolType, test::ServerBinding.GetUri(), true, Origin::User);
    upClientEndpoint->RegisterMediator(spPeerMediator.get());
    bool const result = upClientEndpoint->ScheduleConnect(std::move(address));
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
