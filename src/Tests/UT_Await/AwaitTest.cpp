//------------------------------------------------------------------------------------------------
#include "../../Components/Await/Await.hpp"
#include "../../Utilities/Message.hpp"
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

constexpr NodeUtils::NodeIdType ClientId = 0x12345678;
constexpr NodeUtils::NodeIdType ServerId = 0xFFFFFFFF;
constexpr Command::Type Command = Command::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";
constexpr std::uint32_t Nonce = 9999;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, AwaitObjectSingleResponseTest)
{
    CMessage const request(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);
    
    Await::CMessageObject awaitObject(request, test::ServerId);

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Unfulfilled);

    auto const initialResponse = awaitObject.GetResponse();
    EXPECT_FALSE(initialResponse);

    CMessage const response(
        test::ServerId, test::ClientId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce);
    
    auto const updateStatus = awaitObject.UpdateResponse(response);
    EXPECT_EQ(updateStatus, Await::Status::Fulfilled);

    auto const updateResponse = awaitObject.GetResponse();
    ASSERT_TRUE(updateResponse);

    EXPECT_EQ(updateResponse->GetSourceId(), test::ServerId);
    EXPECT_EQ(updateResponse->GetDestinationId(), test::ClientId);
    EXPECT_FALSE(updateResponse->GetAwaitId());
    EXPECT_EQ(updateResponse->GetCommandType(), test::Command);
    EXPECT_EQ(updateResponse->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(updateResponse->GetNonce(), test::Nonce + 1);
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, AwaitObjectMultipleResponseTest)
{
    CMessage const request(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);
    
    NodeUtils::NodeIdType const peerOneId = 0xAAAAAAAA;
    NodeUtils::NodeIdType const peerTwoId = 0xBBBBBBBB;
    Await::CMessageObject awaitObject(request, {test::ServerId, peerOneId, peerTwoId});

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Unfulfilled);

    auto const initialResponse = awaitObject.GetResponse();
    EXPECT_FALSE(initialResponse);

    CMessage const serverResponse(
        test::ServerId, test::ClientId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce);
    
    CMessage const peerOneResponse(
        peerOneId, test::ServerId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce);

    CMessage const peerTwoResponse(
        peerTwoId, test::ServerId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce);

    auto updateStatus = awaitObject.UpdateResponse(serverResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    updateStatus = awaitObject.UpdateResponse(peerOneResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    updateStatus = awaitObject.UpdateResponse(peerTwoResponse);
    EXPECT_EQ(updateStatus, Await::Status::Fulfilled);

    auto const updateResponse = awaitObject.GetResponse();
    ASSERT_TRUE(updateResponse);

    EXPECT_EQ(updateResponse->GetSourceId(), test::ServerId);
    EXPECT_EQ(updateResponse->GetDestinationId(), test::ClientId);
    EXPECT_FALSE(updateResponse->GetAwaitId());
    EXPECT_EQ(updateResponse->GetCommandType(), test::Command);
    EXPECT_EQ(updateResponse->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(updateResponse->GetNonce(), test::Nonce + 1);
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, ExpiredAwaitObjectNoResponsesTest)
{
    CMessage const request(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);
    
    Await::CMessageObject awaitObject(request, test::ServerId);

    std::this_thread::sleep_for(Await::Timeout);

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Fulfilled);

    auto const response = awaitObject.GetResponse();
    ASSERT_TRUE(response);

    EXPECT_EQ(response->GetSourceId(), test::ServerId);
    EXPECT_EQ(response->GetDestinationId(), test::ClientId);
    EXPECT_FALSE(response->GetAwaitId());
    EXPECT_EQ(response->GetCommandType(), test::Command);
    EXPECT_EQ(response->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(response->GetNonce(), test::Nonce + 1);
    EXPECT_GT(response->GetData().size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, ExpiredAwaitObjectSomeResponsesTest)
{
    CMessage const request(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);
    
    NodeUtils::NodeIdType const peerOneId = 0xAAAAAAAA;
    NodeUtils::NodeIdType const peerTwoId = 0xBBBBBBBB;
    Await::CMessageObject awaitObject(request, {test::ServerId, peerOneId, peerTwoId});

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Unfulfilled);

    auto const initialResponse = awaitObject.GetResponse();
    EXPECT_FALSE(initialResponse);

    CMessage const serverResponse(
        test::ServerId, test::ClientId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce);
    
    CMessage const peerTwoResponse(
        peerTwoId, test::ServerId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce);

    auto updateStatus = awaitObject.UpdateResponse(serverResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    updateStatus = awaitObject.UpdateResponse(peerTwoResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    std::this_thread::sleep_for(Await::Timeout);

    updateStatus = awaitObject.GetStatus();
    EXPECT_EQ(updateStatus, Await::Status::Fulfilled);

    auto const response = awaitObject.GetResponse();
    ASSERT_TRUE(response);

    EXPECT_EQ(response->GetSourceId(), test::ServerId);
    EXPECT_EQ(response->GetDestinationId(), test::ClientId);
    EXPECT_FALSE(response->GetAwaitId());
    EXPECT_EQ(response->GetCommandType(), test::Command);
    EXPECT_EQ(response->GetPhase(), test::ResponsePhase);
    EXPECT_EQ(response->GetNonce(), test::Nonce + 1);
    EXPECT_GT(response->GetData().size(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, ExpiredAwaitObjectLateResponsesTest)
{
    CMessage const request(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);
    
    Await::CMessageObject awaitObject(request, test::ServerId);

    std::this_thread::sleep_for(Await::Timeout);

    auto const initialStatus = awaitObject.GetStatus();
    EXPECT_EQ(initialStatus, Await::Status::Fulfilled);

    auto const initialResponse = awaitObject.GetResponse();
    ASSERT_TRUE(initialResponse);

    std::uint32_t const initialResponseSize = initialResponse->GetData().size();

    CMessage const latePeerResponse(
        test::ServerId, test::ClientId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce);
    
    auto const updateStatus = awaitObject.UpdateResponse(latePeerResponse);
    EXPECT_EQ(updateStatus, Await::Status::Fulfilled);

    auto const updateResponse = awaitObject.GetResponse();
    ASSERT_TRUE(updateResponse);

    std::uint32_t const updateResponseSize = updateResponse->GetData().size();

    EXPECT_EQ(initialResponseSize, updateResponseSize);
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, AwaitObjectUnexpectedResponsesTest)
{
    CMessage const request(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);
    
    Await::CMessageObject awaitObject(request, test::ServerId);

    CMessage const unexpectedPeerResponse(
        0x12345678, test::ClientId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce);
    
    auto const updateStatus = awaitObject.UpdateResponse(unexpectedPeerResponse);
    EXPECT_EQ(updateStatus, Await::Status::Unfulfilled);

    auto const updateResponse = awaitObject.GetResponse();
    EXPECT_FALSE(updateResponse);
}

//------------------------------------------------------------------------------------------------

TEST(AwaitSuite, FulfilledAwaitTest)
{
    Await::CObjectContainer awaiting;

    CMessage const request(
        test::ClientId, test::ServerId,
        test::Command, test::RequestPhase,
        test::Message, test::Nonce);
    
    auto const key = awaiting.PushRequest(request, {test::ServerId, 0xAAAAAAAA, 0xBBBBBBBB});
    ASSERT_GT(key, std::uint32_t(0));

    CMessage const response(
        test::ServerId, 0x01234567,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce,
        Message::BoundAwaitId{
            Message::AwaitBinding::Destination, key});

    CMessage const responseA(
        0xAAAAAAAA, test::ServerId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce,
        Message::BoundAwaitId{
            Message::AwaitBinding::Destination, key});

    CMessage const responseB(
        0xBBBBBBBB, test::ServerId,
        test::Command, test::ResponsePhase,
        test::Message, test::Nonce,
        Message::BoundAwaitId{
            Message::AwaitBinding::Destination, key});
    
    bool result = false;
    result = awaiting.PushResponse(response);
    EXPECT_TRUE(result);

    result = awaiting.PushResponse(responseA);
    EXPECT_TRUE(result);

    result = awaiting.PushResponse(responseB);
    EXPECT_TRUE(result);

    auto const fulfilled = awaiting.GetFulfilled();
    EXPECT_GT(fulfilled.size(), std::uint32_t(0));

    for (auto const& message: fulfilled) {
        auto const decrypted = message.Decrypt(message.GetData(), message.GetData().size());
        ASSERT_NE(decrypted, std::nullopt);
    }
}

//------------------------------------------------------------------------------------------------