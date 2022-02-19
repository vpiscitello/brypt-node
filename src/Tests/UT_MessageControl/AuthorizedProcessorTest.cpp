//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Interfaces/SecurityStrategy.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class StrategyStub;

using MessageContextProvider = std::function<std::optional<MessageContext>()>;
Peer::Registration GenerateCaptureRegistration(
    std::optional<ApplicationMessage>& optCapturedMessage, MessageContextProvider const& getMessageContext);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

constexpr Handler::Type Handler = Handler::Type::Election;
constexpr std::uint8_t RequestPhase = 0;
constexpr std::uint8_t ResponsePhase = 1;
constexpr std::string_view Message = "Hello World!";

constexpr Network::Endpoint::Identifier const EndpointIdentifier = 1;
constexpr Network::Protocol const EndpointProtocol = Network::Protocol::TCP;

Network::RemoteAddress const RemoteClientAddress(Network::Protocol::TCP, "127.0.0.1:35217", false);

constexpr std::uint32_t Iterations = 10000;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::StrategyStub : public ISecurityStrategy
{
public:
    StrategyStub() {}

    virtual Security::Strategy GetStrategyType() const override { return Security::Strategy::Invalid; }
    virtual Security::Role GetRoleType() const override { return Security::Role::Initiator; }
    virtual Security::Context GetContextType() const override { return Security::Context::Unique; }
    virtual std::size_t GetSignatureSize() const override { return 0; }

    virtual std::uint32_t GetSynchronizationStages() const override { return 0; }
    virtual Security::SynchronizationStatus GetSynchronizationStatus() const override
        { return Security::SynchronizationStatus::Processing; }
    virtual Security::SynchronizationResult PrepareSynchronization() override
        { return { Security::SynchronizationStatus::Processing, {} }; }
    virtual Security::SynchronizationResult Synchronize(Security::ReadableView) override
        { return { Security::SynchronizationStatus::Processing, {} }; }

    virtual Security::OptionalBuffer Encrypt(Security::ReadableView buffer, std::uint64_t) const override
        { return Security::Buffer(buffer.begin(), buffer.end()); }
    virtual Security::OptionalBuffer Decrypt(Security::ReadableView buffer, std::uint64_t) const override
        { return Security::Buffer(buffer.begin(), buffer.end()); }

    virtual std::int32_t Sign(Security::Buffer&) const override { return 0; }
    virtual Security::VerificationStatus Verify(Security::ReadableView) const override
        { return Security::VerificationStatus::Success; }

private: 
    virtual std::int32_t Sign(Security::ReadableView, Security::Buffer&) const override { return 0; }
    virtual Security::OptionalBuffer GenerateSignature(Security::ReadableView, Security::ReadableView) const override
        { return {}; }
};

//----------------------------------------------------------------------------------------------------------------------

TEST(AuthorizedProcessorSuite, SingleMessageCollectionTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    AuthorizedProcessor processor(test::ServerIdentifier, {}, spRegistrar);
    std::optional<ApplicationMessage> optCapturedMessage;

    // Create a peer representing a connection to a client.
    auto const spPeerProxy = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spPeerProxy->AttachSecurityStrategy<InvokeContext::Test>(std::make_unique<local::StrategyStub>());

    // Register an endpoint with the peer that will capture any messages sent through it.
    spPeerProxy->RegisterSilentEndpoint<InvokeContext::Test>(
        local::GenerateCaptureRegistration(
            optCapturedMessage,
            [&spPeerProxy] ()  -> auto { return spPeerProxy->GetMessageContext(test::EndpointIdentifier); }));

    // Get the message context for the endpoint that was registered.
    auto const optMessageContext = spPeerProxy->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optMessageContext);
    
    // Generate an application message to represent a request sent from a client. 
    auto const optRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(*optMessageContext)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetCommand(test::Handler, test::RequestPhase)
        .SetPayload(test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optRequest);

    // Use the authorized processor to collect the request. During runtime this would be called  through the peer's 
    // ScheduleReceive method. 
    processor.CollectMessage(spPeerProxy, *optMessageContext, optRequest->GetPack());

    // Verify that the processor correctly queued the message to be processed by the the main event loop.
    EXPECT_EQ(processor.MessageCount(), std::uint32_t(1));
    
    // Pop the queued request to verify it was properly handled.
    auto const optAssociatedMessage = processor.GetNextMessage();
    EXPECT_EQ(processor.MessageCount(), std::uint32_t(0));    
    ASSERT_TRUE(optAssociatedMessage);
    auto& [wpAssociatedPeer, request] = *optAssociatedMessage;

    // Verify that the sent request is the message that was pulled of the processor's queue.
    EXPECT_EQ(optRequest->GetPack(), request.GetPack());

    // Build an application message to represent the response to the client's request.
    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(*optMessageContext)
        .SetSource(*test::ServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetCommand(test::Handler, test::ResponsePhase)
        .SetPayload(test::Message)
        .ValidatedBuild();

    // Obtain the peer associated the reqest.
    if (auto const spAssociatedPeer = wpAssociatedPeer.lock(); spAssociatedPeer) {
        // Verify the peer that was used to send the request matches the peer that was associated with the message.
        EXPECT_EQ(spAssociatedPeer, spPeerProxy);
        
        // Send a message through the peer to further verify that it is correct.
        EXPECT_TRUE(spAssociatedPeer->ScheduleSend(test::EndpointIdentifier, optResponse->GetPack()));
    }

    // Verify that the response passed through the capturing endpoint and matches the correct message. 
    ASSERT_TRUE(optCapturedMessage);
    EXPECT_EQ(optCapturedMessage->GetPack(), optResponse->GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

TEST(AuthorizedProcessorSuite, MultipleMessageCollectionTest)
{
    auto const spRegistrar = std::make_shared<Scheduler::Registrar>();
    AuthorizedProcessor processor(test::ServerIdentifier, {}, spRegistrar);
    std::optional<ApplicationMessage> optCapturedMessage;

    // Create a peer representing a connection to a client.
    auto const spPeerProxy = Peer::Proxy::CreateInstance(test::ClientIdentifier);
    spPeerProxy->AttachSecurityStrategy<InvokeContext::Test>(std::make_unique<local::StrategyStub>());

    // Register an endpoint with the peer that will capture any messages sent through it.
    spPeerProxy->RegisterSilentEndpoint<InvokeContext::Test>(
        local::GenerateCaptureRegistration(
            optCapturedMessage,
            [&spPeerProxy] () -> auto { return spPeerProxy->GetMessageContext(test::EndpointIdentifier); }));

    // Get the message context for the endpoint that was registered.
    auto const optMessageContext = spPeerProxy->GetMessageContext(test::EndpointIdentifier);
    ASSERT_TRUE(optMessageContext);
    
    // Use the processor to collect several messages to verify they are queued correctly. 
    for (std::uint32_t count = 0; count < test::Iterations; ++count) {
        // Generate an application message to represent a request sent from a client. 
        auto const optRequest = Message::Application::Parcel::GetBuilder()
            .SetContext(*optMessageContext)
            .SetSource(test::ClientIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetCommand(test::Handler, test::RequestPhase)
            .SetPayload(test::Message)
            .ValidatedBuild();
        ASSERT_TRUE(optRequest);

        // Use the authorized processor to collect the request. During runtime this would be called  through the peer's
        // ScheduleReceive method. 
        processor.CollectMessage(spPeerProxy, *optMessageContext, optRequest->GetPack());
    }

    // Verify that the processor correctly queued the messages to be processed by the the main  event loop.
    std::uint32_t expectedQueueCount = test::Iterations;
    EXPECT_EQ(processor.MessageCount(), expectedQueueCount);
    
    // While there are messages to processed in the authorized processor's queue validate the
    // processor's functionality and state.
    while (auto const optAssociatedMessage = processor.GetNextMessage()) {
        --expectedQueueCount;
        EXPECT_EQ(processor.MessageCount(), expectedQueueCount);        
        ASSERT_TRUE(optAssociatedMessage);
        auto& [wpAssociatedPeer, request] = *optAssociatedMessage;

        // Build an application message to represent the response to the client's request.
        auto const optResponse = Message::Application::Parcel::GetBuilder()
            .SetContext(*optMessageContext)
            .SetSource(*test::ServerIdentifier)
            .SetDestination(test::ClientIdentifier)
            .SetCommand(test::Handler, test::ResponsePhase)
            .SetPayload(test::Message)
            .ValidatedBuild();

        // Obtain the peer associated the reqest.
        if (auto const spAssociatedPeer = wpAssociatedPeer.lock(); spAssociatedPeer) {
            // Verify the peer that was used to send the request matches the peer that was associated with the message.
            EXPECT_EQ(spAssociatedPeer, spPeerProxy);
            // Send a message through the peer to further verify that it is correct.
            EXPECT_TRUE(spAssociatedPeer->ScheduleSend(test::EndpointIdentifier, optResponse->GetPack()));
        }

        // Verify that the response passed through the capturing endpoint and matches the correct  message. 
        ASSERT_TRUE(optCapturedMessage);
        EXPECT_EQ(optCapturedMessage->GetPack(), optResponse->GetPack());
    }

    // Verify the processor's message queue has been depleted. 
    EXPECT_EQ(processor.MessageCount(), std::uint32_t(0));
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Registration local::GenerateCaptureRegistration(
    std::optional<ApplicationMessage>& optCapturedMessage, MessageContextProvider const& getMessageContext)
{
    Peer::Registration registration(
        test::EndpointIdentifier,
        test::EndpointProtocol,
        test::RemoteClientAddress,
        [&optCapturedMessage, getMessageContext] (auto const&, auto&& message) -> bool
        {
            auto const optMessage = Message::Application::Parcel::GetBuilder()
                .SetContext(*getMessageContext())
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            EXPECT_TRUE(optMessage);
            optCapturedMessage = optMessage;
            return true;
        });
    
    return registration;
}

//----------------------------------------------------------------------------------------------------------------------
