//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Components/Await/Await.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
#include "../../Utilities/NodeUtils.hpp"
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

BryptIdentifier::CContainer const ClientId(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ServerId(BryptIdentifier::Generate());

constexpr Command::Type Command = Command::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";
constexpr std::uint32_t Nonce = 9999;

constexpr Endpoints::EndpointIdType const identifier = 1;
constexpr Endpoints::TechnologyType const technology = Endpoints::TechnologyType::TCP;
CMessageContext const context(identifier, technology);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, AwaitObjectSingleResponseTest)
{
    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    Await::CMessageObject awaitObject(*optRequest, test::ServerId);

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Unfulfilled);

    auto const initialResponse = awaitObject.GetResponse();
    EXPECT_FALSE(initialResponse);

    OptionalMessage const optResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ServerId)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    auto const updateStatus = awaitObject.UpdateResponse(*optResponse);
    EXPECT_EQ(updateStatus, Await::Status::Fulfilled);

    auto const optUpdateResponse = awaitObject.GetResponse();
    ASSERT_TRUE(optUpdateResponse);

    EXPECT_EQ(optUpdateResponse->GetSource(), test::ServerId);
    EXPECT_EQ(optUpdateResponse->GetDestination(), test::ClientId);
    EXPECT_FALSE(optUpdateResponse->GetAwaitingKey());
    EXPECT_EQ(optUpdateResponse->GetCommandType(), test::Command);
    EXPECT_EQ(optUpdateResponse->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(optUpdateResponse->GetNonce(), test::Nonce + 1);
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, AwaitObjectMultipleResponseTest)
{
    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    BryptIdentifier::CContainer const peerOneId(BryptIdentifier::Generate());
    BryptIdentifier::CContainer const peerTwoId(BryptIdentifier::Generate());
    Await::CMessageObject awaitObject(*optRequest, { test::ServerId, peerOneId, peerTwoId });

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Unfulfilled);

    auto const initialResponse = awaitObject.GetResponse();
    EXPECT_FALSE(initialResponse);

    OptionalMessage const optServerResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ServerId)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    OptionalMessage const optPeerOneResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(peerOneId)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    OptionalMessage const optPeerTwoResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(peerTwoId)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    auto updateStatus = awaitObject.UpdateResponse(*optServerResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    updateStatus = awaitObject.UpdateResponse(*optPeerOneResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    updateStatus = awaitObject.UpdateResponse(*optPeerTwoResponse);
    EXPECT_EQ(updateStatus, Await::Status::Fulfilled);

    auto const optUpdateResponse = awaitObject.GetResponse();
    ASSERT_TRUE(optUpdateResponse);

    EXPECT_EQ(optUpdateResponse->GetSource(), test::ServerId);
    EXPECT_EQ(optUpdateResponse->GetDestination(), test::ClientId);
    EXPECT_FALSE(optUpdateResponse->GetAwaitingKey());
    EXPECT_EQ(optUpdateResponse->GetCommandType(), test::Command);
    EXPECT_EQ(optUpdateResponse->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(optUpdateResponse->GetNonce(), test::Nonce + 1);
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, ExpiredAwaitObjectNoResponsesTest)
{
    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    Await::CMessageObject awaitObject(*optRequest, test::ServerId);

    std::this_thread::sleep_for(Await::Timeout);

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Fulfilled);

    auto const response = awaitObject.GetResponse();
    ASSERT_TRUE(response);

    EXPECT_EQ(response->GetSource(), test::ServerId);
    EXPECT_EQ(response->GetDestination(), test::ClientId);
    EXPECT_FALSE(response->GetAwaitingKey());
    EXPECT_EQ(response->GetCommandType(), test::Command);
    EXPECT_EQ(response->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(response->GetNonce(), test::Nonce + 1);
    EXPECT_GT(response->GetData().size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, ExpiredAwaitObjectSomeResponsesTest)
{
    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    BryptIdentifier::CContainer const peerOneId(BryptIdentifier::Generate());
    BryptIdentifier::CContainer const peerTwoId(BryptIdentifier::Generate());
    Await::CMessageObject awaitObject(*optRequest, { test::ServerId, peerOneId, peerTwoId });

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Unfulfilled);

    auto const initialResponse = awaitObject.GetResponse();
    EXPECT_FALSE(initialResponse);

    OptionalMessage const optServerResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ServerId)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    OptionalMessage const optPeerTwoResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(peerTwoId)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    auto updateStatus = awaitObject.UpdateResponse(*optServerResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    updateStatus = awaitObject.UpdateResponse(*optPeerTwoResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    std::this_thread::sleep_for(Await::Timeout);

    updateStatus = awaitObject.GetStatus();
    EXPECT_EQ(updateStatus, Await::Status::Fulfilled);

    auto const response = awaitObject.GetResponse();
    ASSERT_TRUE(response);

    EXPECT_EQ(response->GetSource(), test::ServerId);
    EXPECT_EQ(response->GetDestination(), test::ClientId);
    EXPECT_FALSE(response->GetAwaitingKey());
    EXPECT_EQ(response->GetCommandType(), test::Command);
    EXPECT_EQ(response->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(response->GetNonce(), test::Nonce + 1);
    EXPECT_GT(response->GetData().size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, ExpiredAwaitObjectLateResponsesTest)
{
    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    Await::CMessageObject awaitObject(*optRequest, test::ServerId);

    std::this_thread::sleep_for(Await::Timeout);

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Fulfilled);

    auto const initialResponse = awaitObject.GetResponse();
    ASSERT_TRUE(initialResponse);

    std::uint32_t const initialResponseSize = initialResponse->GetData().size();

    OptionalMessage const optLateResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ServerId)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    auto const updateStatus = awaitObject.UpdateResponse(*optLateResponse);
    EXPECT_EQ(updateStatus, Await::Status::Fulfilled);

    auto const optUpdateResponse = awaitObject.GetResponse();
    ASSERT_TRUE(optUpdateResponse);

    std::uint32_t const updateResponseSize = optUpdateResponse->GetData().size();

    EXPECT_EQ(initialResponseSize, updateResponseSize);
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, AwaitObjectUnexpectedResponsesTest)
{
    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    Await::CMessageObject awaitObject(*optRequest, test::ServerId);

    OptionalMessage const optUnexpectedResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(0x12345678)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();
    
    auto const updateStatus = awaitObject.UpdateResponse(*optUnexpectedResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    auto const optUpdateResponse = awaitObject.GetResponse();
    EXPECT_FALSE(optUpdateResponse);
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, FulfilledAwaitTest)
{
    OptionalMessage const optRequest = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::RequestPhase)
        .SetData(test::Message, test::Nonce)
        .ValidatedBuild();

    BryptIdentifier::CContainer const peerOneId(BryptIdentifier::Generate());
    BryptIdentifier::CContainer const peerTwoId(BryptIdentifier::Generate());

    Await::CObjectContainer awaiting;
    auto const key = awaiting.PushRequest(*optRequest, {test::ServerId, peerOneId, peerTwoId});
    ASSERT_GT(key, std::uint32_t(0));

    OptionalMessage const optResponse = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(test::ServerId)
        .SetDestination(test::ClientId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();

    OptionalMessage const optResponseA = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(peerOneId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();

    OptionalMessage const optResponseB = CMessage::Builder()
        .SetMessageContext(test::context)
        .SetSource(peerTwoId)
        .SetDestination(test::ServerId)
        .SetCommand(test::Command, test::ResponsePhase)
        .SetData(test::Message, test::Nonce)
        .BindAwaitingKey(Message::AwaitBinding::Destination, key)
        .ValidatedBuild();
    
    bool result = false;
    result = awaiting.PushResponse(*optResponse);
    EXPECT_TRUE(result);

    result = awaiting.PushResponse(*optResponseA);
    EXPECT_TRUE(result);

    result = awaiting.PushResponse(*optResponseB);
    EXPECT_TRUE(result);

    auto const fulfilled = awaiting.GetFulfilled();
    EXPECT_GT(fulfilled.size(), std::uint32_t(0));

    for (auto const& message: fulfilled) {
        auto const optDecrypted = message.GetDecryptedData();
        ASSERT_TRUE(optDecrypted);
    }
}

//------------------------------------------------------------------------------------------------