//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
//------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

EndpointRegistration GenerateCaptureRegistration(
    std::optional<ApplicationMessage>& optCapturedMessage);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

BryptIdentifier::Container const ClientIdentifier(BryptIdentifier::Generate());
auto const ServerIdentifier = std::make_shared<BryptIdentifier::Container const>(
    BryptIdentifier::Generate());

constexpr Handler::Type Handler = Handler::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

Network::RemoteAddress const RemoteClientAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);

constexpr std::uint32_t Iterations = 10000;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(AuthorizedProcessorSuite, SingleMessageCollectionTest)
{
    AuthorizedProcessor processor(test::ServerIdentifier);
    std::optional<ApplicationMessage> optCapturedMessage;

    // Create a peer representing a connection to a client.
    auto const spBryptPeer = std::make_shared<BryptPeer>(test::ClientIdentifier);

    // Register an endpoint with the peer that will capture any messages sent through it.
    spBryptPeer->RegisterEndpoint(local::GenerateCaptureRegistration(optCapturedMessage));

    // Get the message context for the endpoint that was registered.
    auto const optMessageContext = spBryptPeer->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optMessageContext);
    
    // Generate an application message to represent a request sent from a client. 
    auto const optRequest = ApplicationMessage::Builder()
        .SetMessageContext(*optMessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    // Use the authorized processor to collect the request. During runtime this would be called 
    // through the peer's ScheduleReceive method. 
    processor.CollectMessage(spBryptPeer, *optMessageContext, optRequest->GetPack());

    // Verify that the processor correctly queued the message to be processed by the the main
    // event loop.
    EXPECT_EQ(processor.QueuedMessageCount(), std::uint32_t(1));
    
    // Pop the queued request to verify it was properly handled.
    auto const optAssociatedMessage = processor.GetNextMessage();

    // Verify the queue count was decreased after popping a message.
    EXPECT_EQ(processor.QueuedMessageCount(), std::uint32_t(0));

    // Destructure the associated message to get the provided peer and request.
    ASSERT_TRUE(optAssociatedMessage);
    auto& [wpAssociatedPeer, request] = *optAssociatedMessage;

    // Verify that the sent request is the message that was pulled of the processor's queue.
    EXPECT_EQ(optRequest->GetPack(), request.GetPack());

    // Build an application message to represent the response to the client's request.
    auto const optResponse = ApplicationMessage::Builder()
        .SetMessageContext(*optMessageContext)
        .SetSource(*test::ServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    // Obtain the peer associated the reqest.
    if (auto const spAssociatedPeer = wpAssociatedPeer.lock(); spAssociatedPeer) {
        // Verify that the peer that was used to send the request matches the peer that was
        // associated with the message.
        EXPECT_EQ(spAssociatedPeer, spBryptPeer);
        
        // Send a message through the peer to further verify that it is correct.
        EXPECT_TRUE(spAssociatedPeer->ScheduleSend(test::EndpointIdentifier, optResponse->GetPack()));
    }

    // Verify that the response passed through the capturing endpoint and matches the correct
    // message. 
    ASSERT_TRUE(optCapturedMessage);
    EXPECT_EQ(optCapturedMessage->GetPack(), optResponse->GetPack());
}

//------------------------------------------------------------------------------------------------

TEST(AuthorizedProcessorSuite, MultipleMessageCollectionTest)
{
    AuthorizedProcessor processor(test::ServerIdentifier);
    std::optional<ApplicationMessage> optCapturedMessage;

    // Create a peer representing a connection to a client.
    auto const spBryptPeer = std::make_shared<BryptPeer>(test::ClientIdentifier);

    // Register an endpoint with the peer that will capture any messages sent through it.
    spBryptPeer->RegisterEndpoint(local::GenerateCaptureRegistration(optCapturedMessage));

    // Get the message context for the endpoint that was registered.
    auto const optMessageContext = spBryptPeer->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optMessageContext);
    
    // Use the processor to collect several messages to verify they are queued correctly. 
    for (std::uint32_t count = 0; count < test::Iterations; ++count) {
        // Generate an application message to represent a request sent from a client. 
        auto const optRequest = ApplicationMessage::Builder()
            .SetMessageContext(*optMessageContext)
            .SetSource(test::ClientIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetCommand(test::Handler, test::RequestPhase)
            .SetPayload(test::Message)
            .ValidatedBuild();

        // Use the authorized processor to collect the request. During runtime this would be called 
        // through the peer's ScheduleReceive method. 
        processor.CollectMessage(spBryptPeer, *optMessageContext, optRequest->GetPack());
    }

    // Verify that the processor correctly queued the messages to be processed by the the main
    // event loop.
    std::uint32_t expectedQueueCount = test::Iterations;
    EXPECT_EQ(processor.QueuedMessageCount(), expectedQueueCount);
    
    // While there are messages to processed in the authorized processor's queue validate the
    // processor's functionality and state.
    while (auto const optAssociatedMessage = processor.GetNextMessage()) {
        --expectedQueueCount;

        // Verify the queue count was decreased after popping a message.
        EXPECT_EQ(processor.QueuedMessageCount(), expectedQueueCount);

        // Destructure the associated message to get the provided peer and request.
        ASSERT_TRUE(optAssociatedMessage);
        auto& [wpAssociatedPeer, request] = *optAssociatedMessage;

        // Build an application message to represent the response to the client's request.
        auto const optResponse = ApplicationMessage::Builder()
            .SetMessageContext(*optMessageContext)
            .SetSource(*test::ServerIdentifier)
            .SetDestination(test::ClientIdentifier)
            .SetCommand(test::Handler, test::ResponsePhase)
            .SetPayload(test::Message)
            .ValidatedBuild();

        // Obtain the peer associated the reqest.
        if (auto const spAssociatedPeer = wpAssociatedPeer.lock(); spAssociatedPeer) {
            // Verify that the peer that was used to send the request matches the peer that was
            // associated with the message.
            EXPECT_EQ(spAssociatedPeer, spBryptPeer);
            
            // Send a message through the peer to further verify that it is correct.
            EXPECT_TRUE(spAssociatedPeer->ScheduleSend(test::EndpointIdentifier, optResponse->GetPack()));
        }

        // Verify that the response passed through the capturing endpoint and matches the correct
        // message. 
        ASSERT_TRUE(optCapturedMessage);
        EXPECT_EQ(optCapturedMessage->GetPack(), optResponse->GetPack());
    }

    // Verify the processor's message queue has been depleted. 
    EXPECT_EQ(processor.QueuedMessageCount(), std::uint32_t(0));
}

//------------------------------------------------------------------------------------------------

EndpointRegistration local::GenerateCaptureRegistration(
    std::optional<ApplicationMessage>& optCapturedMessage)
{
    EndpointRegistration registration(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress,
        [&registration, &optCapturedMessage] (auto const&, auto message) -> bool
        {
            auto const optMessage = ApplicationMessage::Builder()
                .SetMessageContext(registration.GetMessageContext())
                .FromEncodedPack(message)
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);
            optCapturedMessage = optMessage;
            return true;
        });
    
    registration.GetWritableMessageContext().BindEncryptionHandlers(
        [] (auto const& buffer, auto) -> Security::Encryptor::result_type
            {  return Security::Buffer(buffer.begin(), buffer.end()); },
        [] (auto const& buffer, auto) -> Security::Decryptor::result_type
            { return Security::Buffer(buffer.begin(), buffer.end()); });

    registration.GetWritableMessageContext().BindSignatureHandlers(
        [] (auto&) -> Security::Signator::result_type
            { return 0; },
        [] (auto const&) -> Security::Verifier::result_type
            { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type
            { return 0; });

    return registration;
}

//------------------------------------------------------------------------------------------------
