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

//------------------------------------------------------------------------------------------------

TEST(CMessageQueue, ConnectionTrackingTest)
{
    CMessageQueue queue;
    // The connection will register it's callback with the queue 
    auto server = local::MakeDirectClient(&queue);
    // We should expect the registered connection size to be one within the queue
    EXPECT_EQ(queue.RegisteredConnections(), std::uint32_t(1));
    // When the connection is destroyed we should expect the registered callbacks
    // to go back to zero.
    server.reset();
    EXPECT_EQ(queue.RegisteredConnections(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

TEST(CMessageQueue, MessageForwardingTest)
{
    CMessageQueue queue;
    auto server = local::MakeDirectServer(&queue);
    auto client = local::MakeDirectClient(&queue);

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    CMessage const request(
        test::ClientId, test::ServerId,
        NodeUtils::CommandType::Election, 0,
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

    options.operation = NodeUtils::ConnectionOperation::Server;

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

    options.operation = NodeUtils::ConnectionOperation::Client;

    return std::make_unique<Connection::CDirect>(sink, options);
}

//------------------------------------------------------------------------------------------------