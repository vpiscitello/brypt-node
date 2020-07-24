//------------------------------------------------------------------------------------------------
#include "../../Components/Command/CommandDefinitions.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/TcpEndpoint.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageQueue/MessageQueue.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/googletest/include/gtest/gtest.h"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CTcpEndpoint> MakeTcpServer(
    IMessageSink* const sink);
std::unique_ptr<Endpoints::CTcpEndpoint> MakeTcpClient(
    IMessageSink* const sink);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr NodeUtils::NodeIdType ServerId = 0x12345678;
constexpr NodeUtils::NodeIdType ClientId = 0x77777777;
constexpr std::string_view TechnologyName = "Direct";
constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::TCP;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";
constexpr std::string_view ClientEntry = "127.0.0.1:35217";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CTcpSuite, ServerMessageForwardingTest)
{
    CMessageQueue queue;

    auto upServer = local::MakeTcpServer(&queue);
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

    OptionalMessage const optConnectResponse = CMessage::Builder()
        .SetMessageContext(optConnectionRequest->GetMessageContext())
        .SetSource(test::ServerId)
        .SetDestination(test::ClientId)
        .SetCommand(Command::Type::Connect, 1)
        .SetData("Connection Approved", optConnectionRequest->GetNonce() + 1)
        .ValidatedBuild();
    
    queue.PushOutgoingMessage(*optConnectResponse);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto const optReceivedConnectResponse = queue.PopIncomingMessage();
    ASSERT_TRUE(optReceivedConnectResponse);
    EXPECT_EQ(optReceivedConnectResponse->GetPack(), optConnectResponse->GetPack());

    OptionalMessage const optElectionRequest = CMessage::Builder()
        .SetMessageContext(optReceivedConnectResponse->GetMessageContext())
        .SetSource(test::ClientId)
        .SetDestination(test::ServerId)
        .SetCommand(Command::Type::Election, 0)
        .SetData("Hello World!", optReceivedConnectResponse->GetNonce() + 1)
        .ValidatedBuild();
    
    queue.PushOutgoingMessage(*optElectionRequest);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optReceivedElectionRequest = queue.PopIncomingMessage();
    ASSERT_TRUE(optReceivedElectionRequest);
    EXPECT_EQ(optReceivedElectionRequest->GetPack(), optElectionRequest->GetPack());

    OptionalMessage const optElectionResponse = CMessage::Builder()
        .SetMessageContext(optReceivedElectionRequest->GetMessageContext())
        .SetSource(test::ServerId)
        .SetDestination(test::ClientId)
        .SetCommand(Command::Type::Election, 1)
        .SetData("Re: Hello World!", optReceivedElectionRequest->GetNonce() + 1)
        .ValidatedBuild();

    queue.PushOutgoingMessage(*optElectionResponse);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optReceivedElectionResponse = queue.PopIncomingMessage();
    ASSERT_TRUE(optReceivedElectionResponse);
    EXPECT_EQ(optReceivedElectionResponse->GetPack(), optElectionResponse->GetPack());
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CTcpEndpoint> local::MakeTcpServer(
    IMessageSink* const sink)
{
    return std::make_unique<Endpoints::CTcpEndpoint>(
        test::ServerId,
        test::Interface,
        Endpoints::OperationType::Server,
        nullptr,
        nullptr,
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
        nullptr,
        nullptr,
        sink);
}

//------------------------------------------------------------------------------------------------