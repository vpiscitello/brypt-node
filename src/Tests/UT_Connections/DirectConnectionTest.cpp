//------------------------------------------------------------------------------------------------
#include "../../Components/Connection/Connection.hpp"
#include "../../Components/Connection/DirectConnection.hpp"
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

std::unique_ptr<Connection::CDirect> MakeDirectServer(
    IMessageSink* const sink);
std::unique_ptr<Connection::CDirect> MakeDirectClient(
    IMessageSink* const sink);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

constexpr NodeUtils::NodeIdType ServerId = 0x12345678;
constexpr NodeUtils::NodeIdType ClientId = 0xFFFFFFFF;
constexpr std::string_view TechnologyName = "Direct";
constexpr NodeUtils::TechnologyType TechnologyType = NodeUtils::TechnologyType::DIRECT;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:3000";
constexpr std::string_view ClientBinding = "*:3001";
constexpr std::string_view ServerEntry = "127.0.0.1:3000";
constexpr std::string_view ClientEntry = "127.0.0.1:3001";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CDirectSuite, ServerLifecycleTest)
{
    CMessageQueue queue;
    auto connection = local::MakeDirectServer(&queue);
    EXPECT_EQ(connection->GetOperation(), NodeUtils::ConnectionOperation::SERVER);

    bool const result = connection->Shutdown();
    EXPECT_TRUE(result);
}

//------------------------------------------------------------------------------------------------

TEST(CDirectSuite, ClientLifecycleTest)
{
    CMessageQueue queue;
    auto connection = local::MakeDirectClient(&queue);
    EXPECT_EQ(connection->GetOperation(), NodeUtils::ConnectionOperation::CLIENT);

    bool const result = connection->Shutdown();
    EXPECT_TRUE(result);
}

//------------------------------------------------------------------------------------------------

TEST(CDirectSuite, ServerMessageForwardingTest)
{
    CMessageQueue queue;
    auto server = local::MakeDirectServer(&queue);
    auto client = local::MakeDirectClient(&queue);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    CMessage const request(
        test::ClientId, test::ServerId,
        NodeUtils::CommandType::ELECTION, 0,
        "Hello World!", 0);
        
    queue.PushOutgoingMessage(test::ServerId, request);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optReceived = queue.PopIncomingMessage();
    ASSERT_TRUE(optReceived);

    EXPECT_EQ(optReceived->GetPack(), request.GetPack());
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Connection::CDirect> local::MakeDirectServer(
    IMessageSink* const sink)
{
    Configuration::TConnectionOptions options(
        test::ClientId,
        test::TechnologyType,
        test::Interface,
        test::ServerBinding);

    options.operation = NodeUtils::ConnectionOperation::SERVER;

    return std::make_unique<Connection::CDirect>(sink, options);
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Connection::CDirect> local::MakeDirectClient(
    IMessageSink* const sink)
{
    Configuration::TConnectionOptions options(
        test::ServerId,
        test::TechnologyType,
        test::Interface,
        test::ClientBinding,
        test::ServerEntry);

    options.operation = NodeUtils::ConnectionOperation::CLIENT;

    return std::make_unique<Connection::CDirect>(sink, options);
}

//------------------------------------------------------------------------------------------------