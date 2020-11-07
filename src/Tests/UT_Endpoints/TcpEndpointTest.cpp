//------------------------------------------------------------------------------------------------
#include "MessageSinkStub.hpp"
#include "SinglePeerMediatorStub.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../Components/Command/CommandDefinitions.hpp"
#include "../../Components/Endpoints/Endpoint.hpp"
#include "../../Components/Endpoints/TcpEndpoint.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageControl/AuthorizedProcessor.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
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
    std::shared_ptr<IPeerMediator> const& spPeerMediator);
std::unique_ptr<Endpoints::CTcpEndpoint> MakeTcpClient(
    std::shared_ptr<IPeerMediator> const& spPeerMediator);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
    BryptIdentifier::Generate());
auto const ServerIdentifier = std::make_shared<BryptIdentifier::CContainer const>(
    BryptIdentifier::Generate());

constexpr Endpoints::TechnologyType TechnologyType = Endpoints::TechnologyType::TCP;
constexpr std::string_view Interface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ServerEntry = "127.0.0.1:35216";

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CTcpSuite, SingleConnectionTest)
{
    // Create a stub message collector that endpoints can forward messages into.
    CMessageSinkStub collector;

    // Create stub peer mediators for the server and client endpoints. Each will store a single 
    // peer for the endpoint and set the peer's receiver to the stub collector. 
    auto const spServerMediator = std::make_shared<CSinglePeerMediatorStub>(
        test::ServerIdentifier, &collector);
    auto const spClientMediator = std::make_shared<CSinglePeerMediatorStub>(
        test::ClientIdentifier, &collector);

    // Make the server endpoint
    auto upServer = local::MakeTcpServer(spServerMediator);
    upServer->ScheduleBind(test::ServerBinding);
    upServer->Startup();

    // Wait a period of time to ensure the server endpoint is spun up
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Make the client endpoint
    auto upClient = local::MakeTcpClient(spClientMediator);
    upClient->ScheduleConnect(test::ServerEntry);
    upClient->Startup();

    // Wait a period of time to ensure the client initiated the connection with the server
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // We expect there to be a connect request in the incoming collector from the client
    auto const optAssociatedConnectRequest = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedConnectRequest);
    auto const& [wpConnectRequestPeer, connectRequest] = *optAssociatedConnectRequest;

    auto const optConnectResponse = CApplicationMessage::Builder()
        .SetMessageContext(connectRequest.GetMessageContext())
        .SetSource(*test::ServerIdentifier)
        .SetDestination(*test::ClientIdentifier)
        .SetCommand(Command::Type::Connect, 1)
        .SetPayload("Connection Approved")
        .ValidatedBuild();
    
    if (auto const spConnectRequestPeer = wpConnectRequestPeer.lock(); spConnectRequestPeer) {
        spConnectRequestPeer->ScheduleSend(
            optConnectResponse->GetMessageContext(), optConnectResponse->GetPack());
    }

    // Wait a period of time to ensure the message has connect response has been sent and received. 
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto const optAssociatedConnectResponse = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedConnectResponse);
    auto const& [wpConnectResponsePeer, connectResponse] = *optAssociatedConnectResponse;

    EXPECT_EQ(connectResponse.GetPack(), optConnectResponse->GetPack());

    auto const optElectionRequest = CApplicationMessage::Builder()
        .SetMessageContext(connectResponse.GetMessageContext())
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Command::Type::Election, 0)
        .SetPayload("Hello World!")
        .ValidatedBuild();

    if (auto const spConnectResponsePeer = wpConnectResponsePeer.lock(); spConnectResponsePeer) {
        spConnectResponsePeer->ScheduleSend(
            optElectionRequest->GetMessageContext(), optElectionRequest->GetPack());
    }

    // Wait a period of time to ensure the message has election request has been sent and received. 
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optAssociatedElectionRequest = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedElectionRequest);
    auto const& [wpElectionRequestPeer, electionRequest] = *optAssociatedElectionRequest;
    EXPECT_EQ(electionRequest.GetPack(), optElectionRequest->GetPack());

    auto const optElectionResponse = CApplicationMessage::Builder()
        .SetMessageContext(electionRequest.GetMessageContext())
        .SetSource(*test::ServerIdentifier)
        .SetDestination(*test::ClientIdentifier)
        .SetCommand(Command::Type::Election, 1)
        .SetPayload("Re: Hello World!")
        .ValidatedBuild();

    if (auto const spElectionRequestPeer = wpElectionRequestPeer.lock(); spElectionRequestPeer) {
        spElectionRequestPeer->ScheduleSend(
            optElectionResponse->GetMessageContext(), optElectionResponse->GetPack());
    }

    // Wait a period of time to ensure the message has election response has been sent and received. 
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto const optAssociatedElectionResponse = collector.PopIncomingMessage();
    ASSERT_TRUE(optAssociatedElectionResponse);
    auto const& [wpElectionResponsePeer, electionResponse] = *optAssociatedElectionResponse;
    EXPECT_EQ(electionResponse.GetPack(), optElectionResponse->GetPack());
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CTcpEndpoint> local::MakeTcpServer(
    std::shared_ptr<IPeerMediator> const& spPeerMediator)
{
    return std::make_unique<Endpoints::CTcpEndpoint>(
        test::ServerIdentifier,
        test::Interface,
        Endpoints::OperationType::Server,
        nullptr,
        spPeerMediator);
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CTcpEndpoint> local::MakeTcpClient(
    std::shared_ptr<IPeerMediator> const& spPeerMediator)
{
    return std::make_unique<Endpoints::CTcpEndpoint>(
        test::ClientIdentifier,
        test::Interface,
        Endpoints::OperationType::Client,
        nullptr,
        spPeerMediator);
}

//------------------------------------------------------------------------------------------------