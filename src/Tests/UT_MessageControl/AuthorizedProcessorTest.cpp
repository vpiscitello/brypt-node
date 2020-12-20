//------------------------------------------------------------------------------------------------
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../Components/BryptPeer/BryptPeer.hpp"
#include "../../Components/Endpoints/EndpointIdentifier.hpp"
#include "../../Components/Endpoints/TechnologyType.hpp"
#include "../../Components/MessageControl/AssociatedMessage.hpp"
#include "../../Components/MessageControl/AuthorizedProcessor.hpp"
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

CEndpointRegistration GenerateCaptureRegistration(
    std::optional<CApplicationMessage>& optCapturedMessage);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const ClientIdentifier(BryptIdentifier::Generate());
BryptIdentifier::CContainer const ServerIdentifier(BryptIdentifier::Generate());

constexpr Command::Type Command = Command::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";

constexpr Endpoints::EndpointIdType const EndpointIdentifier = 1;
constexpr Endpoints::TechnologyType const EndpointTechnology = Endpoints::TechnologyType::TCP;

constexpr std::uint32_t Iterations = 10000;

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

TEST(AuthorizedProcessorSuite, SingleMessageCollectionTest)
{
    CAuthorizedProcessor processor;
    std::optional<CApplicationMessage> optCapturedMessage;

    // Create a peer representing a connection to a client.
    auto const spBryptPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);

    // Register an endpoint with the peer that will capture any messages sent through it.
    spBryptPeer->RegisterEndpoint(local::GenerateCaptureRegistration(optCapturedMessage));

    // Get the message context for the endpoint that was registered.
    auto const optMessageContext = spBryptPeer->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optMessageContext);
    
    // Generate an application message to represent a request sent from a client. 
    auto const optRequest = CApplicationMessage::Builder()
        .SetMessageContext(*optMessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(test::ServerIdentifier)
        .SetCommand(test::Command, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    // Use the authorized processor to collect the request. During runtime this would be called 
    // through the peer's ScheduleReceive method. 
    processor.CollectMessage(spBryptPeer, *optMessageContext, optRequest->GetPack());

    // Verify that the processor correctly queued the message to be processed by the the main
    // event loop.
    EXPECT_EQ(processor.QueuedMessageCount(), std::uint32_t(1));
    
    // Pop the queued request to verify it was properly handled.
    auto const optAssociatedMessage = processor.PopIncomingMessage();

    // Verify the queue count was decreased after popping a message.
    EXPECT_EQ(processor.QueuedMessageCount(), std::uint32_t(0));

    // Destructure the associated message to get the provided peer and request.
    ASSERT_TRUE(optAssociatedMessage);
    auto& [wpAssociatedPeer, request] = *optAssociatedMessage;

    // Verify that the sent request is the message that was pulled of the processor's queue.
    EXPECT_EQ(optRequest->GetPack(), request.GetPack());

    // Build an application message to represent the response to the client's request.
    auto const optResponse = CApplicationMessage::Builder()
        .SetMessageContext(*optMessageContext)
        .SetSource(test::ServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Command, test::ResponsePhase)
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
    CAuthorizedProcessor processor;
    std::optional<CApplicationMessage> optCapturedMessage;

    // Create a peer representing a connection to a client.
    auto const spBryptPeer = std::make_shared<CBryptPeer>(test::ClientIdentifier);

    // Register an endpoint with the peer that will capture any messages sent through it.
    spBryptPeer->RegisterEndpoint(local::GenerateCaptureRegistration(optCapturedMessage));

    // Get the message context for the endpoint that was registered.
    auto const optMessageContext = spBryptPeer->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optMessageContext);
    
    // Use the processor to collect several messages to verify they are queued correctly. 
    for (std::uint32_t count = 0; count < test::Iterations; ++count) {
        // Generate an application message to represent a request sent from a client. 
        auto const optRequest = CApplicationMessage::Builder()
            .SetMessageContext(*optMessageContext)
            .SetSource(test::ClientIdentifier)
            .SetDestination(test::ServerIdentifier)
            .SetCommand(test::Command, test::RequestPhase)
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
    while (auto const optAssociatedMessage = processor.PopIncomingMessage()) {
        --expectedQueueCount;

        // Verify the queue count was decreased after popping a message.
        EXPECT_EQ(processor.QueuedMessageCount(), expectedQueueCount);

        // Destructure the associated message to get the provided peer and request.
        ASSERT_TRUE(optAssociatedMessage);
        auto& [wpAssociatedPeer, request] = *optAssociatedMessage;

        // Build an application message to represent the response to the client's request.
        auto const optResponse = CApplicationMessage::Builder()
            .SetMessageContext(*optMessageContext)
            .SetSource(test::ServerIdentifier)
            .SetDestination(test::ClientIdentifier)
            .SetCommand(test::Command, test::ResponsePhase)
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

CEndpointRegistration local::GenerateCaptureRegistration(
    std::optional<CApplicationMessage>& optCapturedMessage)
{
    CEndpointRegistration registration(
        test::EndpointIdentifier,
        test::EndpointTechnology,
        [&registration, &optCapturedMessage] (auto const&, auto message) -> bool
        {
            auto const optMessage = CApplicationMessage::Builder()
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
            {  return 0; },
        [] (auto const&) -> Security::Verifier::result_type
            { return Security::VerificationStatus::Success; },
        [] () -> Security::SignatureSizeGetter::result_type
            { return 0; });

    return registration;
}

//------------------------------------------------------------------------------------------------
