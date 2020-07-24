//------------------------------------------------------------------------------------------------
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/DirectEndpoint.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageQueue/MessageQueue.hpp"
#include "../../Message/Message.hpp"
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
constexpr NodeUtils::NodeIdType ClientId = 0x77777777;
constexpr std::string_view TechnologyName = "Direct";
constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::Direct;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";
constexpr std::string_view ClientEntry = "127.0.0.1:35217";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

TEST(CMessageQueue, ConnectionTrackingTest)
{
    CMessageQueue queue;
    // The connection will register it's callback with the queue 
    auto upServer = local::MakeDirectServer(&queue);
    auto const serverIdentifier = upServer->GetIdentifier();
    upServer->ScheduleBind(test::ServerBinding);
    upServer->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto upClient = local::MakeDirectClient(&queue);
    auto const clientIdentifier = upClient->GetIdentifier();
    upClient->ScheduleConnect(test::ServerEntry);
    upClient->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_EQ(queue.QueuedMessageCount(), std::uint32_t(1));

    EXPECT_EQ(queue.RegisteredEndpointCount(), std::uint32_t(2));

    EXPECT_TRUE(queue.IsRegistered(serverIdentifier));
    EXPECT_TRUE(queue.IsRegistered(clientIdentifier));

    // We should expect the registered connection size to be one within the queue
    EXPECT_EQ(queue.TrackedPeerCount(), std::uint32_t(1));
    EXPECT_EQ(queue.TrackedPeerCount(serverIdentifier), std::uint32_t(1));
    EXPECT_EQ(queue.TrackedPeerCount(clientIdentifier), std::uint32_t(0));

    upServer.reset();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // When the connection is destroyed we should expect the registered callbacks
    // to only be one.
    EXPECT_EQ(queue.RegisteredEndpointCount(), std::uint32_t(1));
    EXPECT_FALSE(queue.IsRegistered(serverIdentifier));
    EXPECT_TRUE(queue.IsRegistered(clientIdentifier));

    EXPECT_EQ(queue.TrackedPeerCount(), std::uint32_t(0));
    EXPECT_EQ(queue.TrackedPeerCount(serverIdentifier), std::uint32_t(0));
    EXPECT_EQ(queue.TrackedPeerCount(clientIdentifier), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CDirectEndpoint> local::MakeDirectServer(
    IMessageSink* const sink)
{
    return std::make_unique<Endpoints::CDirectEndpoint>(
        test::ServerId,
        test::Interface,
        Endpoints::OperationType::Server,
        nullptr,
        nullptr,
        sink);
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CDirectEndpoint> local::MakeDirectClient(
    IMessageSink* const sink)
{
    return std::make_unique<Endpoints::CDirectEndpoint>(
        test::ClientId,
        test::Interface,
        Endpoints::OperationType::Client,
        nullptr,
        nullptr,
        sink);
}

//------------------------------------------------------------------------------------------------