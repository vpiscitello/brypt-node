//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/PlatformMessage.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Route/MessageHandler.hpp"
#include "Components/Route/Router.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/MessageSink.hpp"
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

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

class InspectableService;
class FailureService;

class InspectableHandler;
class FailingHandler;

Node::Identifier const ClientIdentifier(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

constexpr std::string_view InspectableRoute = "/test/inspectable/standard";
constexpr std::string_view FailingRoute = "/test/inspectable/failing";

constexpr std::uint32_t Iterations = 10000;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class test::InspectableService
{
public:
    InspectableService() : m_handled(0) {}
    void OnHandled() { ++m_handled; }
    [[nodiscard]] std::size_t const& GetHandled() const { return m_handled; }

private:
    std::size_t m_handled;
};

//----------------------------------------------------------------------------------------------------------------------

class test::FailureService
{
public:
    FailureService() : m_failed(0) {}
    void OnFailed() { ++m_failed; }
    [[nodiscard]] std::size_t const& GetFailed() const { return m_failed; }

private:
    std::size_t m_failed;
};

//----------------------------------------------------------------------------------------------------------------------

class test::InspectableHandler : public Route::IMessageHandler
{
public:
    InspectableHandler() = default;

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const&) override;
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const&, Peer::Action::Next&) override;
    // }IMessageHandler

private:
    std::weak_ptr<test::InspectableService> m_wpInspectableService;
};

//----------------------------------------------------------------------------------------------------------------------

class test::FailingHandler : public Route::IMessageHandler
{
public:
    FailingHandler() = default;

    // IMessageHandler{
    [[nodiscard]] virtual bool OnFetchServices(std::shared_ptr<Node::ServiceProvider> const&) override;
    [[nodiscard]] virtual bool OnMessage(Message::Application::Parcel const&, Peer::Action::Next&) override;
    // }IMessageHandler

private:
    std::weak_ptr<test::FailureService> m_wpFailureService;
};

//----------------------------------------------------------------------------------------------------------------------

class AuthorizedProcessorSuite : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        auto const optRequest = Message::Application::Parcel::GetBuilder()
            .SetContext(m_context)
            .SetSource(test::ClientIdentifier)
            .SetDestination(*test::ServerIdentifier)
            .SetRoute(test::InspectableRoute)
            .SetPayload(MessageControl::Test::Message)
            .BindExtension<Message::Application::Extension::Awaitable>(
                Message::Application::Extension::Awaitable::Request, MessageControl::Test::TrackerKey)
            .ValidatedBuild();
        ASSERT_TRUE(optRequest);
        m_request = *optRequest;

        auto const optResponse = Message::Application::Parcel::GetBuilder()
            .SetContext(m_context)
            .SetSource(*test::ServerIdentifier)
            .SetDestination(test::ClientIdentifier)
            .SetRoute(test::InspectableRoute)
            .SetPayload(MessageControl::Test::Message)
            .BindExtension<Message::Application::Extension::Awaitable>(
                Message::Application::Extension::Awaitable::Response, MessageControl::Test::TrackerKey)
            .ValidatedBuild();
        ASSERT_TRUE(optResponse);
        m_response = *optResponse;
    }

    static void TearDownTestSuite()
    {
        m_request = {};
        m_response = {};
        m_context = {};
    }

    void SetUp() override
    {
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

        m_spTaskService = std::make_shared<Scheduler::TaskService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTaskService);

        m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
        m_spServiceProvider->Register(m_spEventPublisher);

        m_spTrackingService = std::make_shared<Awaitable::TrackingService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTrackingService);

        m_spNodeState = std::make_shared<NodeState>(test::ServerIdentifier, Network::ProtocolSet{});
        m_spServiceProvider->Register(m_spNodeState);

        m_spRouter = std::make_shared<Route::Router>();
        m_spServiceProvider->Register(m_spRouter);

        m_spInspectableService = std::make_shared<test::InspectableService>();
        m_spServiceProvider->Register(m_spInspectableService);
        EXPECT_EQ(m_spInspectableService->GetHandled(), std::size_t{ 0 });

        m_spFailureService = std::make_shared<test::FailureService>();
        m_spServiceProvider->Register(m_spFailureService);
        EXPECT_EQ(m_spFailureService->GetFailed(), std::size_t{ 0 });

        EXPECT_TRUE(m_spRouter->Register<test::InspectableHandler>(test::InspectableRoute));
        EXPECT_TRUE(m_spRouter->Register<test::FailingHandler>(test::FailingRoute));
        EXPECT_TRUE(m_spRouter->Initialize(m_spServiceProvider));

        m_spAuthorizedProcessor = std::make_shared<AuthorizedProcessor>(m_spRegistrar, m_spServiceProvider);
        m_spServiceProvider->Register<IMessageSink>(m_spAuthorizedProcessor);

        EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 0 });
        EXPECT_EQ(m_spAuthorizedProcessor->Execute(), std::size_t{ 0 });

        m_spProxy = Peer::Proxy::CreateInstance(test::ClientIdentifier, m_spServiceProvider);
        m_spProxy->AttachSecurityStrategy<InvokeContext::Test>(std::make_unique<MessageControl::Test::SecurityStrategy>());
        m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>(
            MessageControl::Test::EndpointIdentifier,
            MessageControl::Test::EndpointProtocol,
            MessageControl::Test::RemoteClientAddress,
            [this] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                m_optResult = std::get<std::string>(message);
                return true;
            });
    }

    std::optional<Message::Application::Parcel> TranslateApplicationParcelResult()
    {
        if (!m_optResult) { return {}; }

        auto const optContext = m_spProxy->GetMessageContext(MessageControl::Test::EndpointIdentifier);
        if (!optContext) { return {}; }

        auto const optMessage = Message::Application::Parcel::GetBuilder()
            .SetContext(*optContext)
            .FromEncodedPack(*m_optResult)
            .ValidatedBuild();
        EXPECT_TRUE(optMessage);
        EXPECT_EQ(optMessage->Validate(), Message::ValidationStatus::Success);
        return std::move(*optMessage);
    }

    std::optional<Message::Platform::Parcel> TranslatePlatformParcelResult()
    {
        if (!m_optResult) { return {}; }
        
        auto const optContext = m_spProxy->GetMessageContext(MessageControl::Test::EndpointIdentifier);
        if (!optContext) { return {}; }

        auto const optMessage = Message::Platform::Parcel::GetBuilder()
            .SetContext(*optContext)
            .FromEncodedPack(*m_optResult)
            .ValidatedBuild();
        EXPECT_TRUE(optMessage);
        EXPECT_EQ(optMessage->Validate(), Message::ValidationStatus::Success);
        return std::move(*optMessage);
    }

    static Message::Context m_context;
    static Message::Application::Parcel m_request;
    static Message::Application::Parcel m_response;

    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Route::Router> m_spRouter;
    std::shared_ptr<test::InspectableService> m_spInspectableService;
    std::shared_ptr<test::FailureService> m_spFailureService;

    std::shared_ptr<AuthorizedProcessor> m_spAuthorizedProcessor;
    std::shared_ptr<Peer::Proxy> m_spProxy;

    std::optional<std::string> m_optResult;
};

//----------------------------------------------------------------------------------------------------------------------

Message::Context AuthorizedProcessorSuite::m_context = MessageControl::Test::GenerateMessageContext();
Message::Application::Parcel AuthorizedProcessorSuite::m_request = {};
Message::Application::Parcel AuthorizedProcessorSuite::m_response = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectSingleMessageTest)
{
    // Use the authorized processor to collect the request. During runtime this would be called  through the peer's 
    // ScheduleReceive method. 
    EXPECT_TRUE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, m_request.GetPack()));

    // Verify that the processor correctly queued the message to be processed by the the main event loop.
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 1 });
    
    // Pop the queued request to verify it was properly handled.
    auto const optAssociatedMessage = m_spAuthorizedProcessor->GetNextMessage<InvokeContext::Test>();
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 0 });
    ASSERT_TRUE(optAssociatedMessage);
    auto& [wpAssociatedPeer, request] = *optAssociatedMessage;

    // Verify that the sent request is the message that was pulled of the processor's queue.
    EXPECT_EQ(m_request.GetPack(), request.GetPack());

    // Verify the associated peer can be acquired and used to respond. 
    if (auto const spAssociatedPeer = wpAssociatedPeer.lock(); spAssociatedPeer) {
        // Verify the peer that was used to send the request matches the peer that was associated with the message.
        EXPECT_EQ(spAssociatedPeer, m_spProxy);
        
        // Send a message through the peer to further verify that it is correct.
        EXPECT_TRUE(spAssociatedPeer->ScheduleSend(MessageControl::Test::EndpointIdentifier, m_response.GetPack()));
    }

    // Verify that the response passed through the capturing endpoint and matches the correct message. 
    auto const optResponse = TranslateApplicationParcelResult();
    ASSERT_TRUE(optResponse);
    EXPECT_EQ(optResponse->GetPack(), m_response.GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectMultipleMessagesTest)
{
    // Use the processor to collect several messages to verify they are queued correctly. 
    for (std::uint32_t count = 0; count < test::Iterations; ++count) {
        // Use the authorized processor to collect the request. During runtime this would be called  through the peer's
        // ScheduleReceive method. 
        EXPECT_TRUE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, m_request.GetPack()));
    }

    // Verify that the processor correctly queued the messages to be processed by the the main  event loop.
    std::uint32_t expectedQueueCount = test::Iterations;
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), expectedQueueCount);
    
    // While there are messages to processed in the authorized processor's queue validate the
    // processor's functionality and state.
    while (auto const optAssociatedMessage = m_spAuthorizedProcessor->GetNextMessage<InvokeContext::Test>()) {
        --expectedQueueCount;
        EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), expectedQueueCount);     
        ASSERT_TRUE(optAssociatedMessage);
        auto& [wpAssociatedPeer, request] = *optAssociatedMessage;

        // Verify the associated peer can be acquired and used to respond. 
        if (auto const spAssociatedPeer = wpAssociatedPeer.lock(); spAssociatedPeer) {
            // Verify the peer that was used to send the request matches the peer that was associated with the message.
            EXPECT_EQ(spAssociatedPeer, m_spProxy);
            
            // Send a message through the peer to further verify that it is correct.
            EXPECT_TRUE(spAssociatedPeer->ScheduleSend(MessageControl::Test::EndpointIdentifier, m_response.GetPack()));
        }

        // Verify that the response passed through the capturing endpoint and matches the correct  message. 
        auto const optResponse = TranslateApplicationParcelResult();
        ASSERT_TRUE(optResponse);
        EXPECT_EQ(optResponse->GetPack(), m_response.GetPack());
    }

    // Verify the processor's message queue has been depleted. 
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, ProcessorExecutionRoutingSuccessfulHanderTest)
{
    // Use the authorized processor to collect the request. During runtime this would be called  through the peer's 
    // ScheduleReceive method. 
    EXPECT_TRUE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, m_request.GetPack()));

    // Verify that the processor correctly queued the message to be processed by the the main event loop.
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 1 });

    // Currently, the execution cycle for the processor will only handle one message at a time. Tuning how many messages
    // should be processed will be future work. 
    EXPECT_EQ(m_spAuthorizedProcessor->Execute(), std::size_t{ 1 });
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 0 });

    // The authorized processor should route the message to the correct handler when executed. 
    EXPECT_EQ(m_spInspectableService->GetHandled(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, ProcessorExecutionRoutingFailingHanderTest)
{
    auto const optFailingRequest = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(test::FailingRoute)
        .SetPayload(MessageControl::Test::Message)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Request, MessageControl::Test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optFailingRequest);

    // Use the authorized processor to collect the request. During runtime this would be called  through the peer's 
    // ScheduleReceive method. 
    EXPECT_TRUE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optFailingRequest->GetPack()));

    // Verify that the processor correctly queued the message to be processed by the the main event loop.
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 1 });

    // Verify that even if the message handler fails to handle the message, the processor still notifies a message 
    // has been processed. 
    EXPECT_EQ(m_spAuthorizedProcessor->Execute(), std::size_t{ 1 });
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 0 });

    // The authorized processor should route the message to the correct handler when executed. 
    EXPECT_EQ(m_spFailureService->GetFailed(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectApplicationParcelAwaitableResponseTest)
{
    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetDestination(*test::ServerIdentifier)
        .SetSource(test::ClientIdentifier)
        .SetRoute(test::InspectableRoute);

    std::optional<Message::Application::Parcel> optCapturedResponse;
    auto const optTrackerKey = m_spTrackingService->StageRequest(m_spProxy, 
        [&optCapturedResponse] (auto const& response) { optCapturedResponse = response; },
        [&] (auto const&, auto) { ASSERT_FALSE(true); }, builder);
    ASSERT_TRUE(optTrackerKey); // The service should supply a tracker key on success. 
    ASSERT_FALSE(optCapturedResponse);

    EXPECT_EQ(m_spTrackingService->Waiting(), std::size_t{ 1 });

    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(test::InspectableRoute)
        .SetPayload(MessageControl::Test::Message)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Response, *optTrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_TRUE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optResponse->GetPack()));

    // Awaitable responses are routed to the tracking service and will be executed as part of that execution cycle. 
    EXPECT_EQ(m_spAuthorizedProcessor->Execute(), std::size_t{ 0 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 1 });
    EXPECT_EQ(m_spTrackingService->Execute(), std::size_t{ 1 });
    
    EXPECT_EQ(optCapturedResponse, optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectApplicationParcelUnexpectedDestinationTest)
{
    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*test::ServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .SetRoute(test::FailingRoute)
        .SetPayload(MessageControl::Test::Message)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Request, MessageControl::Test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_FALSE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optResponse->GetPack()));
    EXPECT_EQ(m_spAuthorizedProcessor->Execute(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectApplicationParcelUnexpectedAwaitableResponseTest)
{
    auto const optResponse = Message::Application::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetRoute(test::FailingRoute)
        .SetPayload(MessageControl::Test::Message)
        .BindExtension<Message::Application::Extension::Awaitable>(
            Message::Application::Extension::Awaitable::Response, MessageControl::Test::TrackerKey)
        .ValidatedBuild();
    ASSERT_TRUE(optResponse);

    EXPECT_FALSE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optResponse->GetPack()));
    EXPECT_EQ(m_spAuthorizedProcessor->Execute(), std::size_t{ 0 });
    EXPECT_EQ(m_spTrackingService->Ready(), std::size_t{ 0 });
    EXPECT_EQ(m_spTrackingService->Execute(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectPlatformParcelHeartbeatRequestTest)
{
    auto const optHeartbeatRequest = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optHeartbeatRequest);

    // Use the authorized processor to collect the request. During runtime this would be called  through the peer's 
    // ScheduleReceive method. 
    EXPECT_TRUE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optHeartbeatRequest->GetPack()));

    // Verify that the processor did not queue the platform message into the application message queue. 
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 0 });

    // Verify that the response passed through the capturing endpoint and matches the correct message. 
    auto const optResponse = TranslatePlatformParcelResult();
    ASSERT_TRUE(optResponse);

    EXPECT_EQ(optResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(optResponse->GetDestination(), test::ClientIdentifier);
    EXPECT_EQ(optResponse->GetDestinationType(), Message::Destination::Node);
    EXPECT_EQ(optResponse->GetType(), Message::Platform::ParcelType::HeartbeatResponse);
    EXPECT_TRUE(optResponse->GetPayload().IsEmpty());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectPlatformParcelHeartbeatResponseTest)
{
    auto const optHeartbeatResponse = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optHeartbeatResponse);

    // Use the authorized processor to collect the request. During runtime this would be called  through the peer's 
    // ScheduleReceive method. 
    EXPECT_TRUE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optHeartbeatResponse->GetPack()));

    // Verify that the processor did not queue the platform message into the application message queue. 
    EXPECT_EQ(m_spAuthorizedProcessor->MessageCount(), std::uint32_t{ 0 });

    // Currently, no actions are taken when a heatbeat response is received. 
    EXPECT_FALSE(m_optResult);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectPlatformParcelHandshakeMessageTest)
{
    auto const optHandshakeMessage = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optHandshakeMessage);
    EXPECT_TRUE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optHandshakeMessage->GetPack()));

    {
        auto const optHeartbeatRequest = TranslatePlatformParcelResult();
        ASSERT_TRUE(optHeartbeatRequest);

        EXPECT_EQ(optHeartbeatRequest->GetSource(), *test::ServerIdentifier);
        EXPECT_EQ(optHeartbeatRequest->GetDestination(), test::ClientIdentifier);
        EXPECT_EQ(optHeartbeatRequest->GetDestinationType(), Message::Destination::Node);
        EXPECT_EQ(optHeartbeatRequest->GetType(), Message::Platform::ParcelType::HeartbeatRequest);
        EXPECT_TRUE(optHeartbeatRequest->GetPayload().IsEmpty());
    }

    // Currently, heartbeat requests are also sent in response to handshake messages with a destination. Future work
    // may include changing this to cause key renegotiation. 
    m_optResult.reset();
    
    auto const optHandshakeMessageWithDestination = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(MessageControl::Test::Message)
        .ValidatedBuild();
    ASSERT_TRUE(optHandshakeMessageWithDestination);
    EXPECT_TRUE(
        m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optHandshakeMessageWithDestination->GetPack()));
    
    {
        auto const optHeartbeatRequest = TranslatePlatformParcelResult();
        ASSERT_TRUE(optHeartbeatRequest);

        EXPECT_EQ(optHeartbeatRequest->GetSource(), *test::ServerIdentifier);
        EXPECT_EQ(optHeartbeatRequest->GetDestination(), test::ClientIdentifier);
        EXPECT_EQ(optHeartbeatRequest->GetDestinationType(), Message::Destination::Node);
        EXPECT_EQ(optHeartbeatRequest->GetType(), Message::Platform::ParcelType::HeartbeatRequest);
        EXPECT_TRUE(optHeartbeatRequest->GetPayload().IsEmpty());
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectPlatformParcelUnexpectedDestinationTest)
{
    auto const optHeartbeatResponse = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(*test::ServerIdentifier)
        .SetDestination(test::ClientIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optHeartbeatResponse);
    EXPECT_FALSE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optHeartbeatResponse->GetPack()));
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectPlatformParcelUnexpectedDestinationTypeTest)
{
    auto const optClusterHeartbeatRequest = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .MakeClusterMessage<InvokeContext::Test>()
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optClusterHeartbeatRequest);
    EXPECT_FALSE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optClusterHeartbeatRequest->GetPack()));

    auto const optNetworkHeartbeatRequest = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .MakeNetworkMessage<InvokeContext::Test>()
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optNetworkHeartbeatRequest);
    EXPECT_FALSE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optNetworkHeartbeatRequest->GetPack()));
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(AuthorizedProcessorSuite, CollectPlatformParcelMissingDestinationTest)
{
    auto const optHeartbeatRequest = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optHeartbeatRequest);
    EXPECT_FALSE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optHeartbeatRequest->GetPack()));

    auto const optHeartbeatResponse = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_context)
        .SetSource(test::ClientIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optHeartbeatResponse);
    EXPECT_FALSE(m_spAuthorizedProcessor->CollectMessage(m_spProxy, m_context, optHeartbeatResponse->GetPack()));
}

//----------------------------------------------------------------------------------------------------------------------

bool test::InspectableHandler::OnFetchServices(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    m_wpInspectableService = spServiceProvider->Fetch<test::InspectableService>();
    if (m_wpInspectableService.expired()) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool test::InspectableHandler::OnMessage(Message::Application::Parcel const& message, Peer::Action::Next&)
{
    if (message.GetRoute() != test::InspectableRoute) { return false; }
    if (auto const spInspectable = m_wpInspectableService.lock(); spInspectable) {
        spInspectable->OnHandled();
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool test::FailingHandler::OnFetchServices(std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    m_wpFailureService = spServiceProvider->Fetch<test::FailureService>();
    if (m_wpFailureService.expired()) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool test::FailingHandler::OnMessage(Message::Application::Parcel const& message, Peer::Action::Next&)
{
    if (message.GetRoute() != test::FailingRoute) { return false; }
    if (auto const spFailureService = m_wpFailureService.lock(); spFailureService) {
        spFailureService->OnFailed();
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
