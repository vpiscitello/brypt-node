//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Components/Await/AwaitDefinitions.hpp"
#include "../../Components/Await/ResponseTracker.hpp"
#include "../../Components/Await/TrackingManager.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string_view>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

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
constexpr std::uint32_t Nonce = 9999;

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;
CMessageContext const MessageContext(EndpointIdentifier, EndpointTechnology);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, SingleResponseTest)
{
    std::optional<CMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optFulfilledResponse] (CMessage const& message) -> bool
        {
            CMessage::ValidationStatus status = message.Validate();
            if (status != CMessage::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = message;
            return true;
        });

    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    Await::CResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Unfulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_FALSE(bInitialSendSuccess);
    EXPECT_FALSE(optFulfilledResponse);

    OptionalMessage const optResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    EXPECT_EQ(tracker.UpdateResponse(*optResponse), Await::ResponseStatus::Fulfilled);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSource(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitingKey());
    EXPECT_EQ(optFulfilledResponse->GetCommandType(), test::Command);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(optFulfilledResponse->GetNonce(), test::Nonce + 1);
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, MultipleResponseTest)
{
    std::optional<CMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optFulfilledResponse] (CMessage const& message) -> bool
        {
            CMessage::ValidationStatus status = message.Validate();
            if (status != CMessage::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = message;
            return true;
        });

    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
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

    OptionalMessage const optServerResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    OptionalMessage const optPeerOneResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*spFirstIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    OptionalMessage const optPeerTwoResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*spSecondIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    EXPECT_EQ(tracker.UpdateResponse(*optServerResponse), Await::ResponseStatus::Unfulfilled);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerOneResponse), Await::ResponseStatus::Unfulfilled);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerTwoResponse), Await::ResponseStatus::Fulfilled);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSource(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitingKey());
    EXPECT_EQ(optFulfilledResponse->GetCommandType(), test::Command);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(optFulfilledResponse->GetNonce(), test::Nonce + 1);
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, ExpiredNoResponsesTest)
{
    std::optional<CMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optFulfilledResponse] (CMessage const& message) -> bool
        {
            CMessage::ValidationStatus status = message.Validate();
            if (status != CMessage::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = message;
            return true;
        });

    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    Await::CResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    std::this_thread::sleep_for(Await::CResponseTracker::ExpirationPeriod);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Fulfilled);

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bInitialSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSource(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitingKey());
    EXPECT_EQ(optFulfilledResponse->GetCommandType(), test::Command);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(optFulfilledResponse->GetNonce(), test::Nonce + 1);
    EXPECT_GT(optFulfilledResponse->GetData().size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, ExpiredSomeResponsesTest)
{
    std::optional<CMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optFulfilledResponse] (CMessage const& message) -> bool
        {
            CMessage::ValidationStatus status = message.Validate();
            if (status != CMessage::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = message;
            return true;
        });

    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
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

    OptionalMessage const optServerResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    OptionalMessage const optPeerTwoResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*spSecondIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    EXPECT_EQ(tracker.UpdateResponse(*optServerResponse), Await::ResponseStatus::Unfulfilled);
    EXPECT_EQ(tracker.UpdateResponse(*optPeerTwoResponse), Await::ResponseStatus::Unfulfilled);

    std::this_thread::sleep_for(Await::CResponseTracker::ExpirationPeriod);

    bool const bUpdateSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bUpdateSendSuccess);

    ASSERT_TRUE(optFulfilledResponse);
    EXPECT_EQ(optFulfilledResponse->GetSource(), *test::spServerIdentifier);
    EXPECT_EQ(optFulfilledResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_FALSE(optFulfilledResponse->GetAwaitingKey());
    EXPECT_EQ(optFulfilledResponse->GetCommandType(), test::Command);
    EXPECT_EQ(optFulfilledResponse->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(optFulfilledResponse->GetNonce(), test::Nonce + 1);
    EXPECT_GT(optFulfilledResponse->GetData().size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, ExpiredLateResponsesTest)
{
    std::optional<CMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optFulfilledResponse] (CMessage const& message) -> bool
        {
            CMessage::ValidationStatus status = message.Validate();
            if (status != CMessage::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = message;
            return true;
        });

    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    Await::CResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);
    std::this_thread::sleep_for(Await::CResponseTracker::ExpirationPeriod);

    auto const initialResponseStatus = tracker.CheckResponseStatus();
    EXPECT_EQ(initialResponseStatus, Await::ResponseStatus::Fulfilled);
    EXPECT_EQ(tracker.GetResponseCount(), std::uint32_t(0));

    bool const bInitialSendSuccess = tracker.SendFulfilledResponse();
    EXPECT_TRUE(bInitialSendSuccess);
    EXPECT_TRUE(optFulfilledResponse);

    OptionalMessage const optLateResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    EXPECT_EQ(tracker.UpdateResponse(*optLateResponse), Await::ResponseStatus::Completed);
    EXPECT_EQ(tracker.GetResponseCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CResponseTrackerSuite, UnexpectedResponsesTest)
{
    std::optional<CMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optFulfilledResponse] (CMessage const& message) -> bool
        {
            CMessage::ValidationStatus status = message.Validate();
            if (status != CMessage::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = message;
            return true;
        });

    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    Await::CResponseTracker tracker(spClientPeer, *optRequest, test::spServerIdentifier);

    OptionalMessage const optUnexpectedResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(0x12345678)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
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
    std::optional<CMessage> optFulfilledResponse = {};
    auto const spClientPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);
    spClientPeer->RegisterEndpoint(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&optFulfilledResponse] (CMessage const& message) -> bool
        {
            CMessage::ValidationStatus status = message.Validate();
            if (status != CMessage::ValidationStatus::Success) {
                return false;
            }
            optFulfilledResponse = message;
            return true;
        });

    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
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

    OptionalMessage const optResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*test::spServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();

    OptionalMessage const optFirstResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*spFirstIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();

    OptionalMessage const optSecondResponse = CMessage::Builder()
        .SetMessageContext(test::MessageContext)
        .SetSource(*spSecondIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();
    
    EXPECT_TRUE(manager.PushResponse(*optResponse));
    EXPECT_TRUE(manager.PushResponse(*optFirstResponse));
    EXPECT_TRUE(manager.PushResponse(*optSecondResponse));

    manager.ProcessFulfilledRequests();
    EXPECT_TRUE(optFulfilledResponse);
}

//------------------------------------------------------------------------------------------------