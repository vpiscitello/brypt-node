//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Await/AwaitDefinitions.hpp"
#include "Components/Await/ResponseTracker.hpp"
#include "Components/Await/TrackingManager.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Service.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string_view>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------
using namespace std::chrono_literals;
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

MessageContext GenerateMessageContext();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
auto const spServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

constexpr Handler::Type Handler = Handler::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

Network::RemoteAddress const RemoteClientAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(ResponseTrackerSuite, SingleResponseTest)
{
    MessageContext const context = local::GenerateMessageContext();

    std::optional<ApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress,
        [&context, &optFulfilledResponse] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) { return false; }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    Await::ResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Unfulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bInitialSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);

    auto const optResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    EXPECT_EQ(tracker.UpdateResponse(*optResponse), Await::UpdateStatus::Fulfilled);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSourceIdentifier(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestinationIdentifier(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitTrackerKey());
    EXPECT_EQ(optFulfilledResponse->GetCommand(), test::Handler);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ResponseTrackerSuite, MultipleResponseTest)
{
    MessageContext const context = local::GenerateMessageContext();
    
    std::optional<ApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress, 
        [&context, &optFulfilledResponse] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) { return false; }

            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    auto const spFirstIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
    auto const spSecondIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

    Await::ResponseTracker tracker(
        spClientPeer, *optRequest, { test::spServerIdentifier, spFirstIdentifier, spSecondIdentifier });

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Unfulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bInitialSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);

    auto const optServerResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const optPeerOneResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spFirstIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const optPeerTwoResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSecondIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    EXPECT_EQ(tracker.UpdateResponse(*optServerResponse), Await::UpdateStatus::Success);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerOneResponse), Await::UpdateStatus::Success);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerTwoResponse), Await::UpdateStatus::Fulfilled);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSourceIdentifier(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestinationIdentifier(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitTrackerKey());
    EXPECT_EQ(optFulfilledResponse->GetCommand(), test::Handler);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ResponseTrackerSuite, ExpiredNoResponsesTest)
{
    MessageContext const context = local::GenerateMessageContext();
    
    std::optional<ApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress,
        [&context, &optFulfilledResponse] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) { return false; }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    Await::ResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    std::this_thread::sleep_for(Await::ResponseTracker::ExpirationPeriod + 1ms);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Fulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bInitialSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSourceIdentifier(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestinationIdentifier(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitTrackerKey());
    EXPECT_EQ(optFulfilledResponse->GetCommand(), test::Handler);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
    EXPECT_GT(optFulfilledResponse->GetPayload().size(), std::uint32_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ResponseTrackerSuite, ExpiredSomeResponsesTest)
{
    MessageContext const context = local::GenerateMessageContext();

    std::optional<ApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress,
        [&context, &optFulfilledResponse] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    auto const spFirstIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
    auto const spSecondIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

    Await::ResponseTracker tracker(
        spClientPeer, *optRequest, { test::spServerIdentifier, spFirstIdentifier, spSecondIdentifier });

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Unfulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bInitialSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);

    auto const optServerResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const optPeerTwoResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSecondIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    EXPECT_EQ(tracker.UpdateResponse(*optServerResponse), Await::UpdateStatus::Success);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerTwoResponse), Await::UpdateStatus::Success);

    std::this_thread::sleep_for(Await::ResponseTracker::ExpirationPeriod + 1ms);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSourceIdentifier(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestinationIdentifier(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitTrackerKey());
    EXPECT_EQ(optFulfilledResponse->GetCommand(), test::Handler);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
    EXPECT_GT(optFulfilledResponse->GetPayload().size(), std::uint32_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ResponseTrackerSuite, ExpiredLateResponsesTest)
{
    MessageContext const context = local::GenerateMessageContext();

    std::optional<ApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress,
        [&context, &optFulfilledResponse] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) { return false; }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    Await::ResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);
    std::this_thread::sleep_for(Await::ResponseTracker::ExpirationPeriod + 1ms);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Fulfilled);
    EXPECT_EQ(tracker.GetResponseCount(), std::uint32_t(0));

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bInitialSendSuccess);
    EXPECT_TRUE(optFulfilledResponse);

    auto const optLateResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    EXPECT_EQ(tracker.UpdateResponse(*optLateResponse), Await::UpdateStatus::Expired);
    EXPECT_EQ(tracker.GetResponseCount(), std::uint32_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

TEST(ResponseTrackerSuite, UnexpectedResponsesTest)
{
    MessageContext const context = local::GenerateMessageContext();

    std::optional<ApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress,
        [&context, &optFulfilledResponse] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) { return false; }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    Await::ResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    auto const optUnexpectedResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(0x12345678)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    EXPECT_EQ(tracker.UpdateResponse(*optUnexpectedResponse), Await::UpdateStatus::Unexpected);
    EXPECT_EQ(tracker.GetResponseCount(), std::uint32_t(0));

    auto const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bUpdateSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(TrackingManagerSuite, ProcessFulfilledResponseTest)
{
    MessageContext const context = local::GenerateMessageContext();
    
    std::optional<ApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spClientPeer->RegisterSilentEndpoint<InvokeContext::Test>(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress,
        [&context, &optFulfilledResponse] ([[maybe_unused]] auto const& destination, auto&& message) -> bool
        {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) { return false; }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const spFirstIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
    auto const spSecondIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

    auto const spScheduler = std::make_shared<Scheduler::Service>();
    Await::TrackingManager manager(spScheduler);
    auto const key = manager.PushRequest(
        spClientPeer, *optRequest, { test::spServerIdentifier, spFirstIdentifier, spSecondIdentifier });
    EXPECT_GT(key, std::uint32_t(0));

    auto const optResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();

    auto const optFirstResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spFirstIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();

    auto const optSecondResponse = ApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSecondIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();
    
    EXPECT_TRUE(manager.PushResponse(*optResponse));
    EXPECT_TRUE(manager.PushResponse(*optFirstResponse));
    EXPECT_TRUE(manager.PushResponse(*optSecondResponse));

    EXPECT_EQ(manager.ProcessFulfilledRequests(), 1);
    EXPECT_TRUE(optFulfilledResponse);
}

//----------------------------------------------------------------------------------------------------------------------

MessageContext local::GenerateMessageContext()
{
    MessageContext context(test::EndpointIdentifier, test::EndpointProtocol);

    context.BindEncryptionHandlers(
        [] (auto const& buffer, auto) -> Security::Encryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); },
        [] (auto const& buffer, auto) -> Security::Decryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); });

    context.BindSignatureHandlers(
        [] (auto&) -> Security::Signator::result_type  { return 0; },
        [] (auto const&) -> Security::Verifier::result_type { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type { return 0; });

    return context;
}

//----------------------------------------------------------------------------------------------------------------------
