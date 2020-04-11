//------------------------------------------------------------------------------------------------
#include "../../Components/Command/CommandDefinitions.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/DirectEndpoint.hpp"
#include "../../Components/MessageQueue/MessageQueue.hpp"
#include "../../Configuration/Configuration.hpp"
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

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CDirectEndpoint> MakeDirectServer(
    IMessageSink* const sink);
std::unique_ptr<Endpoints::CDirectEndpoint> MakeDirectClient(
    IMessageSink* const sink);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr NodeUtils::NodeIdType ServerId = 0x12345678;
constexpr NodeUtils::NodeIdType ClientId = 0xFFFFFFFF;
constexpr std::string_view TechnologyName = "Direct";
constexpr NodeUtils::TechnologyType TechnologyType = NodeUtils::TechnologyType::Direct;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:3000";
constexpr std::string_view ClientBinding = "*:3001";
constexpr std::string_view ServerEntry = "127.0.0.1:3000";
constexpr std::string_view ClientEntry = "127.0.0.1:3001";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CDirectSuite, ServerCommunicationTest)
{
    CMessageQueue queue;
    auto upServer = local::MakeDirectServer(&queue);
    upServer->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto upClient = local::MakeDirectClient(&queue);
    upClient->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // We expect there to be a connect request in the incoming queue from the client
    auto const optConnectionRequest = queue.PopIncomingMessage();
    ASSERT_TRUE(optConnectionRequest);

    CMessage const connectResponse(
        test::ServerId, test::ClientId,
        Command::Type::Connect, 1,
        "Connection Approved", 1);
    queue.PushOutgoingMessage(test::ClientId, connectResponse);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto const optConnectResponse = queue.PopIncomingMessage();
    ASSERT_TRUE(optConnectResponse);
    EXPECT_EQ(optConnectResponse->GetPack(), connectResponse.GetPack());

    CMessage const electionRequest(
        test::ClientId, test::ServerId,
        Command::Type::Election, 0,
        "Hello World!", 0);
    queue.PushOutgoingMessage(test::ServerId, electionRequest);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optElectionRequest = queue.PopIncomingMessage();
    ASSERT_TRUE(optElectionRequest);
    EXPECT_EQ(optElectionRequest->GetPack(), electionRequest.GetPack());

    CMessage const electionResponse(
        test::ServerId, test::ClientId,
        Command::Type::Election, 1,
        "Re: Hello World!", 0);
    queue.PushOutgoingMessage(test::ClientId, electionResponse);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optElectionResponse = queue.PopIncomingMessage();
    ASSERT_TRUE(optElectionResponse);
    EXPECT_EQ(optElectionResponse->GetPack(), electionResponse.GetPack());
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CDirectEndpoint> local::MakeDirectServer(
    IMessageSink* const sink)
{
    Configuration::TEndpointOptions options(
        test::ServerId,
        test::TechnologyType,
        test::Interface,
        test::ServerBinding);

    options.operation = NodeUtils::EndpointOperation::Server;

    return std::make_unique<Endpoints::CDirectEndpoint>(sink, options);
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CDirectEndpoint> local::MakeDirectClient(
    IMessageSink* const sink)
{
    Configuration::TEndpointOptions options(
        test::ClientId,
        test::TechnologyType,
        test::Interface,
        test::ClientBinding,
        test::ServerEntry);

    options.operation = NodeUtils::EndpointOperation::Client;

    return std::make_unique<Endpoints::CDirectEndpoint>(sink, options);
}

//------------------------------------------------------------------------------------------------