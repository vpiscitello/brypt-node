//------------------------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------------------------

TEST(CMessageQueue, ConnectionTrackingTest)
{
    CMessageQueue queue;
    // The connection will register it's callback with the queue 
    auto upServer = local::MakeDirectServer(&queue);
    upServer->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto upClient = local::MakeDirectClient(&queue);
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