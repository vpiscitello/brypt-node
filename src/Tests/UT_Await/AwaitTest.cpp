//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Await/AwaitDefinitions.hpp"
#include "Components/Await/ResponseTracker.hpp"
#include "Components/Await/TrackingManager.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string_view>
#include <thread>
//------------------------------------------------------------------------------------------------
using namespace std::chrono_literals;
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

CMessageContext GenerateMessageContext();

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
auto const spServerIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
    BryptIdentifier::Generate());

constexpr Command::Type Command = Command::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, SingleResponseTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    std::optional<CApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&context, &optFulfilledResponse] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            auto const optMessage = CApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(message)
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    Await::CResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Unfulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bInitialSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);

    auto const optResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    EXPECT_EQ(tracker.UpdateResponse(*optResponse), Await::ResponseStatus::Fulfilled);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSourceIdentifier(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestinationIdentifier(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitTrackerKey());
    EXPECT_EQ(optFulfilledResponse->GetCommand(), test::Command);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, MultipleResponseTest)
{
    CMessageContext const context = local::GenerateMessageContext();
    
    std::optional<CApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&context, &optFulfilledResponse] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            auto const optMessage = CApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(message)
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    auto const spFirstIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
        BryptIdentifier::Generate());
    auto const spSecondIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
        BryptIdentifier::Generate());

    Await::CResponseTracker tracker(
        spClientPeer,
        *optRequest,
        { test::spServerIdentifier, spFirstIdentifier, spSecondIdentifier });

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Unfulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bInitialSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);

    auto const optServerResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const optPeerOneResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spFirstIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const optPeerTwoResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSecondIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    EXPECT_EQ(tracker.UpdateResponse(*optServerResponse), Await::ResponseStatus::Unfulfilled);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerOneResponse), Await::ResponseStatus::Unfulfilled);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerTwoResponse), Await::ResponseStatus::Fulfilled);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSourceIdentifier(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestinationIdentifier(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitTrackerKey());
    EXPECT_EQ(optFulfilledResponse->GetCommand(), test::Command);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, ExpiredNoResponsesTest)
{
    CMessageContext const context = local::GenerateMessageContext();
    
    std::optional<CApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&context, &optFulfilledResponse] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            auto const optMessage = CApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(message)
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    Await::CResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    std::this_thread::sleep_for(Await::CResponseTracker::ExpirationPeriod + 1ms);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Fulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bInitialSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSourceIdentifier(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestinationIdentifier(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitTrackerKey());
    EXPECT_EQ(optFulfilledResponse->GetCommand(), test::Command);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
    EXPECT_GT(optFulfilledResponse->GetPayload().size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, ExpiredSomeResponsesTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    std::optional<CApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&context, &optFulfilledResponse] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            auto const optMessage = CApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(message)
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    auto const spFirstIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
        BryptIdentifier::Generate());
    auto const spSecondIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
        BryptIdentifier::Generate());

    Await::CResponseTracker tracker(
        spClientPeer,
        *optRequest,
        { test::spServerIdentifier, spFirstIdentifier, spSecondIdentifier });

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Unfulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bInitialSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);

    auto const optServerResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const optPeerTwoResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSecondIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    EXPECT_EQ(tracker.UpdateResponse(*optServerResponse), Await::ResponseStatus::Unfulfilled);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerTwoResponse), Await::ResponseStatus::Unfulfilled);

    std::this_thread::sleep_for(Await::CResponseTracker::ExpirationPeriod + 1ms);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSourceIdentifier(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestinationIdentifier(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitTrackerKey());
    EXPECT_EQ(optFulfilledResponse->GetCommand(), test::Command);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
    EXPECT_GT(optFulfilledResponse->GetPayload().size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, ExpiredLateResponsesTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    std::optional<CApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&context, &optFulfilledResponse] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            auto const optMessage = CApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(message)
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    Await::CResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);
    std::this_thread::sleep_for(Await::CResponseTracker::ExpirationPeriod + 1ms);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Fulfilled);
    EXPECT_EQ(tracker.GetResponseCount(), std::uint32_t(0));

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bInitialSendSuccess);
    EXPECT_TRUE(optFulfilledResponse);

    auto const optLateResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    EXPECT_EQ(tracker.UpdateResponse(*optLateResponse), Await::ResponseStatus::Completed);
    EXPECT_EQ(tracker.GetResponseCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, UnexpectedResponsesTest)
{
    CMessageContext const context = local::GenerateMessageContext();

    std::optional<CApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&context, &optFulfilledResponse] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            auto const optMessage = CApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(message)
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    Await::CResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    auto const optUnexpectedResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(0x12345678)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    
    EXPECT_EQ(tracker.UpdateResponse(*optUnexpectedResponse), Await::ResponseStatus::Unfulfilled);
    EXPECT_EQ(tracker.GetResponseCount(), std::uint32_t(0));

    auto const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bUpdateSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);
}

//------------------------------------------------------------------------------------------------

TEST(CTrackingManagerSuite, ProcessFulfilledResponseTest)
{
    CMessageContext const context = local::GenerateMessageContext();
    
    std::optional<CApplicationMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&context, &optFulfilledResponse] (
            [[maybe_unused]] auto const& destination, std::string_view message) -> bool
        {
            auto const optMessage = CApplicationMessage::Builder()
                .SetMessageContext(context)
                .FromEncodedPack(message)
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);

            Message::ValidationStatus status = optMessage->Validate();
            if (status != Message::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = optMessage;
            return true;
        });

    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    auto const spFirstIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
        BryptIdentifier::Generate());
    auto const spSecondIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
        BryptIdentifier::Generate());

    Await::CTrackingManager manager;
    auto const key = manager.PushRequest(
        spClientPeer,
        *optRequest,
        { test::spServerIdentifier, spFirstIdentifier, spSecondIdentifier });
    EXPECT_GT(key, std::uint32_t(0));

    auto const optResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();

    auto const optFirstResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spFirstIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();

    auto const optSecondResponse = CApplicationMessage::Builder()
        .SetMessageContext(context)
        .SetSource(*spSecondIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetPayload(test::Message)
        .BindAwaitTracker(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();
    
    EXPECT_TRUE(manager.PushResponse(*optResponse));
    EXPECT_TRUE(manager.PushResponse(*optFirstResponse));
    EXPECT_TRUE(manager.PushResponse(*optSecondResponse));

    manager.ProcessFulfilledRequests();
    EXPECT_TRUE(optFulfilledResponse);
}

//------------------------------------------------------------------------------------------------

CMessageContext local::GenerateMessageContext()
{
    CMessageContext context(test::EndpointIdentifier, test::EndpointTechnology);

    context.BindEncryptionHandlers(
        [] (auto const& buffer, auto) -> Security::Encryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); },
        [] (auto const& buffer, auto) -> Security::Decryptor::result_type 
            { return Security::Buffer(buffer.begin(), buffer.end()); });

    context.BindSignatureHandlers(
        [] (auto&) -> Security::Signator::result_type 
            { return 0; },
        [] (auto const&) -> Security::Verifier::result_type 
            { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type 
            { return 0; });

    return context;
}

//------------------------------------------------------------------------------------------------
