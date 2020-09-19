//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Components/Command/CommandDefinitions.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/TcpEndpoint.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageControl/MessageCollector.hpp"
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

auto const spClientIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
    BryptIdentifier::Generate());
auto const spServerIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
    BryptIdentifier::Generate());

constexpr std::string_view TechnologyName = "Direct";
constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::TCP;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35220";
constexpr std::string_view ClientBinding = "*:35221";
constexpr std::string_view ServerEntry = "127.0.0.1:35220";
constexpr std::string_view ClientEntry = "127.0.0.1:35221";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CTcpSuite, ServerMessageForwardingTest)
{
    CMessageCollector collector;

    auto upServer = local::MakeTcpServer(&collector);
    upServer->ScheduleBind(test::ServerBinding);
    upServer->Startup();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    auto upClient = local::MakeTcpClient(&collector);
    upClient->ScheduleConnect(test::ServerEntry);
    upClient->Startup();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // We expect there to be a connect request in the incoming collector from the client
    auto const optAssociatedConnectRequest = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedConnectRequest);
    auto const& [wpConnectRequestPeer, connectRequest] = *optAssociatedConnectRequest;

    OptionalMessage const optConnectResponse = CMessage::Builder()
        .SetMessageContext(connectRequest.GetMessageContext())
        .SetSource(*test::spServerIdentifier)
        .SetDestination(*test::spClientIdentifier)
        .SetCommand(Command::Type::Connect, 1)
        .SetData("Connection Approved", connectRequest.GetNonce() + 1)
        .ValidatedBuild();
    
    if (auto const spConnectRequestPeer = wpConnectRequestPeer.lock(); spConnectRequestPeer) {
        spConnectRequestPeer->ScheduleSend(*optConnectResponse);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto const optAssociatedConnectResponse = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedConnectResponse);
    auto const& [wpConnectResponsePeer, connectResponse] = *optAssociatedConnectResponse;

    EXPECT_EQ(connectResponse.GetPack(), optConnectResponse->GetPack());

    OptionalMessage const optElectionRequest = CMessage::Builder()
        .SetMessageContext(connectResponse.GetMessageContext())
        .SetSource(*test::spClientIdentifier)
        .SetDestination(*test::spServerIdentifier)
        .SetCommand(Command::Type::Election, 0)
        .SetData("Hello World!", connectResponse.GetNonce() + 1)
        .ValidatedBuild();

    if (auto const spConnectResponsePeer = wpConnectResponsePeer.lock(); spConnectResponsePeer) {
        spConnectResponsePeer->ScheduleSend(*optElectionRequest);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optAssociatedElectionRequest = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedElectionRequest);
    auto const& [wpElectionRequestPeer, electionRequest] = *optAssociatedElectionRequest;
    EXPECT_EQ(electionRequest.GetPack(), optElectionRequest->GetPack());

    OptionalMessage const optElectionResponse = CMessage::Builder()
        .SetMessageContext(electionRequest.GetMessageContext())
        .SetSource(*test::spServerIdentifier)
        .SetDestination(*test::spClientIdentifier)
        .SetCommand(Command::Type::Election, 1)
        .SetData("Re: Hello World!", electionRequest.GetNonce() + 1)
        .ValidatedBuild();

    if (auto const spElectionRequestPeer = wpElectionRequestPeer.lock(); spElectionRequestPeer) {
        spElectionRequestPeer->ScheduleSend(*optElectionResponse);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optAssociatedElectionResponse = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedElectionResponse);
    auto const& [wpElectionResponsePeer, electionResponse] = *optAssociatedElectionResponse;
    EXPECT_EQ(electionResponse.GetPack(), optElectionResponse->GetPack());
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CTcpEndpoint> local::MakeTcpServer(
    IMessageSink* const sink)
{
    return std::make_unique<Endpoints::CTcpEndpoint>(
        test::spServerIdentifier,
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
        test::spClientIdentifier,
        test::Interface,
        Endpoints::OperationType::Client,
        nullptr,
        nullptr,
        sink);
}

//------------------------------------------------------------------------------------------------