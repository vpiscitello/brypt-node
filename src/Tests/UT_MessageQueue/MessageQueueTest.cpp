//------------------------------------------------------------------------------------------------
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/DirectEndpoint.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageQueue/MessageQueue.hpp"
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
    upServer->ScheduleBind(test::ServerBinding);
    upServer->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto upClient = local::MakeDirectClient(&queue);
    upClient->ScheduleConnect(test::ServerEntry);
    upClient->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // We should expect the registered connection size to be one within the queue
    EXPECT_EQ(queue.RegisteredPeers(), std::uint32_t(1));

    // When the connection is destroyed we should expect the registered callbacks
    // to go back to zero.
    upServer.reset();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_EQ(queue.RegisteredPeers(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CDirectEndpoint> local::MakeDirectServer(
    IMessageSink* const sink)
{
    return std::make_unique<Endpoints::CDirectEndpoint>(
        test::ServerId,
        test::Interface,
        Endpoints::OperationType::Server,
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
        sink);
}

//------------------------------------------------------------------------------------------------