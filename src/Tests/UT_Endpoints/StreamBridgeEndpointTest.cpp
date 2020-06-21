//------------------------------------------------------------------------------------------------
#include "../../Components/Command/CommandDefinitions.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/StreamBridgeEndpoint.hpp"
#include "../../Components/Endpoints/TcpEndpoint.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageQueue/MessageQueue.hpp"
#include "../../Utilities/CallbackIteration.hpp"
#include "../../Utilities/Message.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

using namespace StreamBridge;

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CStreamBridgeEndpoint> MakeStreamBridgeServer(
    IMessageSink* const sink);
std::unique_ptr<Endpoints::CTcpEndpoint> MakeTcpClient(
    IMessageSink* const sink);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr NodeUtils::NodeIdType ServerId = 0x12345678;
constexpr NodeUtils::NodeIdType ClientId = 0xFFFFFFFF;
constexpr std::string_view TechnologyName = "Direct";
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";
constexpr std::string_view ClientEntry = "127.0.0.1:35217";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CStreamBridgeSuite, ServerCommunicationTest)
{
    CMessageQueue queue;

    auto upServer = local::MakeStreamBridgeServer(&queue);
    upServer->ScheduleBind(test::ServerBinding);
    upServer->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto upClient = local::MakeTcpClient(&queue);
    upClient->ScheduleConnect(test::ServerEntry);
    upClient->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // We expect there to be a connect request in the incoming queue from the client
    auto const optConnectionRequest = queue.PopIncomingMessage();
    ASSERT_TRUE(optConnectionRequest);

    CMessage const connectResponse(
        optConnectionRequest->GetMessageContext(),
        test::ServerId, test::ClientId,
        Command::Type::Connect, 1,
        "Connection Approved", 1);
    queue.PushOutgoingMessage(connectResponse);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto const optConnectResponse = queue.PopIncomingMessage();
    ASSERT_TRUE(optConnectResponse);
    EXPECT_EQ(optConnectResponse->GetPack(), connectResponse.GetPack());

    CMessage const electionRequest(
        optConnectResponse->GetMessageContext(),
        test::ClientId, test::ServerId,
        Command::Type::Election, 0,
        "Hello World!", 0);
    queue.PushOutgoingMessage(electionRequest);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optElectionRequest = queue.PopIncomingMessage();
    ASSERT_TRUE(optElectionRequest);
    EXPECT_EQ(optElectionRequest->GetPack(), electionRequest.GetPack());

    CMessage const electionResponse(
        optElectionRequest->GetMessageContext(),
        test::ServerId, test::ClientId,
        Command::Type::Election, 1,
        "Re: Hello World!", 0);
    queue.PushOutgoingMessage(electionResponse);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optElectionResponse = queue.PopIncomingMessage();
    ASSERT_TRUE(optElectionResponse);
    EXPECT_EQ(optElectionResponse->GetPack(), electionResponse.GetPack());
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CStreamBridgeEndpoint> local::MakeStreamBridgeServer(
    IMessageSink* const sink)
{
    return std::make_unique<Endpoints::CStreamBridgeEndpoint>(
        test::ServerId,
        test::Interface,
        Endpoints::OperationType::Server,
        sink);
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CTcpEndpoint> local::MakeTcpClient(
    IMessageSink* const sink)
{
    return std::make_unique<Endpoints::CTcpEndpoint>(
        test::ClientId,
        test::Interface,
        Endpoints::OperationType::Client,
        sink);
}

//------------------------------------------------------------------------------------------------