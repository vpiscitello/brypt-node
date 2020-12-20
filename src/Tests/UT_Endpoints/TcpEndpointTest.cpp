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
#include <gtest/gtest.h>
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

constexpr std::uint32_t Iterations = 100;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(CTcpSuite, SingleConnectionTest)
{
    // Create the server resources. The peer mediator stub will store a single BryptPeer 
    // representing the client.
    auto upServerProcessor = std::make_unique<CMessageSinkStub>(test::ServerIdentifier);
    auto const spServerMediator = std::make_shared<CSinglePeerMediatorStub>(
        test::ServerIdentifier, upServerProcessor.get());
    auto upServerEndpoint = local::MakeTcpServer(spServerMediator);
    EXPECT_EQ(upServerEndpoint->GetInternalType(), Endpoints::TechnologyType::TCP);
    EXPECT_EQ(upServerEndpoint->GetOperation(), Endpoints::OperationType::Server);
    EXPECT_EQ(upServerEndpoint->GetEntry(), test::ServerEntry);

    // Wait a period of time to ensure the server endpoint is spun up
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Create the client resources. The peer mediator stub will store a single BryptPeer 
    // representing the server.
    auto upClientProcessor = std::make_unique<CMessageSinkStub>(test::ClientIdentifier);
    auto const spClientMediator = std::make_shared<CSinglePeerMediatorStub>(
        test::ClientIdentifier, upClientProcessor.get());
    auto upClientEndpoint = local::MakeTcpClient(spClientMediator);
    EXPECT_EQ(upClientEndpoint->GetInternalType(), Endpoints::TechnologyType::TCP);
    EXPECT_EQ(upClientEndpoint->GetOperation(), Endpoints::OperationType::Client);

    // Wait a period of time to ensure the client initiated the connection with the server
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Verify that the client endpoint used the connection declaration method on the mediator.
    EXPECT_TRUE(upServerProcessor->ReceviedHeartbeatRequest());
    EXPECT_TRUE(upClientProcessor->ReceviedHeartbeatResponse());

    // Acquire the peer associated with the server endpoint from the perspective of the client.
    auto const spClientPeer = spClientMediator->GetPeer();
    ASSERT_TRUE(spClientPeer);

    // Acqure the message context for the client peer's endpoint.
    auto const optClientContext = spClientPeer->GetMessageContext(
        upClientEndpoint->GetEndpointIdentifier());
    ASSERT_TRUE(optClientContext);

    // Build an application message to be sent to the server.
    auto const optQueryRequest = CApplicationMessage::Builder()
        .SetMessageContext(*optClientContext)
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(Command::Type::Query, 0)
        .SetPayload("Query Request")
        .ValidatedBuild();
    ASSERT_TRUE(optQueryRequest);

    // Acquire the peer associated with the server endpoint from the perspective of the client.
    auto const spServerPeer = spServerMediator->GetPeer();
    ASSERT_TRUE(spServerPeer);

    // Acqure the message context for the client peer's endpoint.
    auto const optServerContext = spServerPeer->GetMessageContext(
        upServerEndpoint->GetEndpointIdentifier());
    ASSERT_TRUE(optServerContext);

    // Build an application message to be sent to the client.
    auto const optQueryResponse = CApplicationMessage::Builder()
        .SetMessageContext(*optServerContext)
        .SetSource(*test::ServerIdentifier)
        .SetDestination(*test::ClientIdentifier)
        .SetCommand(Command::Type::Query, 1)
        .SetPayload("Query Response")
        .ValidatedBuild();
    ASSERT_TRUE(optQueryResponse);

    std::string const request = optQueryRequest->GetPack();
    std::string const response = optQueryResponse->GetPack();

    // Send the initial request to the server through the peer.
    spClientPeer->ScheduleSend(optClientContext->GetEndpointIdentifier(), request);

    // For some number of iterations enter request/response cycle using the peers obtained
    // from the processors. 
    for (std::uint32_t iterations = 0; iterations < test::Iterations; ++iterations) {
        // Wait a period of time to ensure the request has been sent and received. 
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Handle the reciept of a request sent to the server.
        {
            auto const optAssociatedRequest = upServerProcessor->GetNextMessage();
            ASSERT_TRUE(optAssociatedRequest);

            // Verify the received request matches the one that was sent through the client.
            auto const& [wpRequestPeer, message] = *optAssociatedRequest;
            EXPECT_EQ(message.GetPack(), request);

            // Send a response to the client
            if (auto const spRequestPeer = wpRequestPeer.lock(); spRequestPeer) {
                spRequestPeer->ScheduleSend(
                    message.GetContext().GetEndpointIdentifier(), response);
            }
        }

        // Wait a period of time to ensure the response has been sent and received. 
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Handle the reciept of a response sent to the client.
        {
            auto const optAssociatedResponse = upClientProcessor->GetNextMessage();
            ASSERT_TRUE(optAssociatedResponse);

            // Verify the received response matches the one that was sent through the server.
            auto const& [wpResponsePeer, message] = *optAssociatedResponse;
            EXPECT_EQ(message.GetPack(), response);

            // Send a request to the server.
            if (auto const spResponsePeer = wpResponsePeer.lock(); spResponsePeer) {
                spResponsePeer->ScheduleSend(
                    message.GetContext().GetEndpointIdentifier(), request);
            }
        }
    }

    EXPECT_EQ(upServerProcessor->InvalidMessageCount(), std::uint32_t(0));
    EXPECT_EQ(upClientProcessor->InvalidMessageCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CTcpEndpoint> local::MakeTcpServer(
    std::shared_ptr<IPeerMediator> const& spPeerMediator)
{
    auto upServerEndpoint = std::make_unique<Endpoints::CTcpEndpoint>(
        test::Interface, Endpoints::OperationType::Server, nullptr, spPeerMediator.get());

    upServerEndpoint->ScheduleBind(test::ServerBinding);
    upServerEndpoint->Startup();

    return upServerEndpoint;
}

//------------------------------------------------------------------------------------------------

std::unique_ptr<Endpoints::CTcpEndpoint> local::MakeTcpClient(
    std::shared_ptr<IPeerMediator> const& spPeerMediator)
{
    auto upClientEndpoint = std::make_unique<Endpoints::CTcpEndpoint>(
        test::Interface, Endpoints::OperationType::Client, nullptr, spPeerMediator.get());

    upClientEndpoint->ScheduleConnect(test::ServerEntry);
    upClientEndpoint->Startup();

    return upClientEndpoint;
}

//------------------------------------------------------------------------------------------------