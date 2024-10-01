//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Message/PlatformMessage.hpp"
#include "Components/Processor/ExchangeProcessor.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Components/Security/CipherService.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/Synchronizer.hpp"
#include "Utilities/InvokeContext.hpp"
#include "Tests/UT_Security/TestHelpers.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class ExchangeResources;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

enum class CaseType : std::uint32_t { Positive, Negative };

class PreparationSynchronizer;
class BasicSynchronizer;

std::string const KeyAgreementName = "kem-kyber768";
std::string const CipherName = "aes-256-ctr";
std::string const HashFunctionName = "sha384";

auto const ClientIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());

constexpr std::string_view ExchangeCloseMessage = "Exchange Success!";

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::ExchangeResources
{
public:
    ExchangeResources(Node::SharedIdentifier const& spSelf, Node::SharedIdentifier const& spTarget);
    
    [[nodiscard]] std::shared_ptr<Processor::Test::ConnectProtocol> const& GetConnectProtocol() const { return m_spConnectProtocol; }
    [[nodiscard]] Message::Context& GetContext() { return m_context; }
    [[nodiscard]] std::shared_ptr<Peer::Proxy> const& GetProxy() const { return m_spProxy; }
    [[nodiscard]] std::unique_ptr<Processor::Test::ExchangeObserver> const& GetObserver() const { return m_upExchangeObserver; }
    [[nodiscard]] std::unique_ptr<ExchangeProcessor> const& GetProcessor() const { return m_upExchangeProcessor; }

    void CreateProcessor(Security::ExchangeRole role);
    void CreateTestProcessor(Security::ExchangeRole role, std::unique_ptr<ISynchronizer>&& upSynchronizer);
    void CreatePreparationSynchronizer(test::CaseType type);
    [[nodiscard]] bool CreateBasicSynchronizer(test::CaseType type, Security::ExchangeRole role);

private:
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Security::CipherService> m_spCipherService;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<Processor::Test::ConnectProtocol> m_spConnectProtocol;
    std::shared_ptr<NodeState> m_spNodeState;
    std::unique_ptr<Processor::Test::ExchangeObserver> m_upExchangeObserver;
    Message::Context m_context;
    std::shared_ptr<Peer::Proxy> m_spProxy;
    std::unique_ptr<ExchangeProcessor> m_upExchangeProcessor;
};

//----------------------------------------------------------------------------------------------------------------------

class test::PreparationSynchronizer : public ISynchronizer
{
public:
    PreparationSynchronizer(test::CaseType type, std::string_view data)
        : m_type(type)
        , m_data(data.begin(), data.end())
    {
    }

    [[nodiscard]] virtual Security::ExchangeRole GetExchangeRole() const override { return Security::ExchangeRole::Initiator; }

    [[nodiscard]] virtual std::uint32_t GetStages() const override { return 1; }
    [[nodiscard]] virtual Security::SynchronizationStatus GetStatus() const override { return GetRequestedTestStatus(); }
    [[nodiscard]] virtual bool Synchronized() const override { return GetRequestedTestStatus() == Security::SynchronizationStatus::Ready; }

    [[nodiscard]] virtual Security::SynchronizationResult Initialize() override
    {
        return { GetRequestedTestStatus(), m_data };
    }

    [[nodiscard]] virtual Security::SynchronizationResult Synchronize(Security::ReadableView buffer) override
    {
        return { GetRequestedTestStatus(), {} };
    }

    [[nodiscard]] virtual std::unique_ptr<Security::CipherPackage> Finalize() override
    {
        switch (m_type) {
            case test::CaseType::Positive: return Security::Test::GenerateCipherPackage();
            case test::CaseType::Negative:
            default: return nullptr;
        }
    }

private: 
    Security::SynchronizationStatus GetRequestedTestStatus() const
    {
        switch (m_type) {
            case test::CaseType::Positive: return Security::SynchronizationStatus::Processing;
            case test::CaseType::Negative:
            default: return Security::SynchronizationStatus::Error;
        }
    }

    test::CaseType m_type;
    Security::Buffer m_data;
};

//----------------------------------------------------------------------------------------------------------------------

class test::BasicSynchronizer : public ISynchronizer
{
public:
    BasicSynchronizer(test::CaseType type, Security::ExchangeRole role, std::string_view data)
        : m_type(type)
        , m_role(role)
        , m_data(data.begin(), data.end())
    {
    }

    [[nodiscard]] virtual Security::ExchangeRole GetExchangeRole() const override { return m_role; }

    [[nodiscard]] virtual std::uint32_t GetStages() const override { return 1; }
    [[nodiscard]] virtual Security::SynchronizationStatus GetStatus() const override { return GetRequestedTestStatus(); }
    [[nodiscard]] virtual bool Synchronized() const override { return GetRequestedTestStatus() == Security::SynchronizationStatus::Ready; }

    [[nodiscard]] virtual Security::SynchronizationResult Initialize() override
    {
        return { Security::SynchronizationStatus::Processing, m_data };
    }
    
    [[nodiscard]] virtual Security::SynchronizationResult Synchronize(Security::ReadableView) override
    { 
        auto buffer = (m_role == Security::ExchangeRole::Initiator) ? m_data : Security::Buffer{};
        return { GetRequestedTestStatus(), std::move(buffer) };
    }

    [[nodiscard]] virtual std::unique_ptr<Security::CipherPackage> Finalize() override
    {
        switch (m_type) {
            case test::CaseType::Positive: return Security::Test::GenerateCipherPackage();
            case test::CaseType::Negative:
            default: return nullptr;
        }
    }

private: 
    Security::SynchronizationStatus GetRequestedTestStatus() const
    {
        switch (m_type) {
            case test::CaseType::Positive: return Security::SynchronizationStatus::Ready;
            case test::CaseType::Negative:
            default: return Security::SynchronizationStatus::Error;
        }
    }

    test::CaseType m_type;
    Security::ExchangeRole m_role;
    Security::Buffer m_data;
};

//----------------------------------------------------------------------------------------------------------------------

class ExchangeProcessorSuite : public testing::Test
{
protected:
    void SetUp() override
    {
        auto optHandshakeMessage = Message::Platform::Parcel::GetBuilder()
            .SetContext(m_client.GetContext())
            .SetSource(*test::ServerIdentifier)
            .SetPayload(Processor::Test::Message)
            .MakeHandshakeMessage()
            .ValidatedBuild();
        ASSERT_TRUE(optHandshakeMessage); 
        m_handshake = std::move(*optHandshakeMessage);
    }

    void SetupCaptureProxies()
    {
        m_server.GetProxy()->RegisterSilentEndpoint<InvokeContext::Test>(
            Processor::Test::EndpointIdentifier,
            Processor::Test::EndpointProtocol,
            Processor::Test::RemoteClientAddress,
            [this] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                auto const optMessage = Message::Platform::Parcel::GetBuilder()
                    .SetContext(m_server.GetContext())
                    .FromEncodedPack(std::get<std::string>(message))
                    .ValidatedBuild();
                EXPECT_TRUE(optMessage);

                Message::ValidationStatus status = optMessage->Validate();
                if (status != Message::ValidationStatus::Success) { return false; }
                m_optResponse = optMessage;
                return true;
            });
        
        auto const optServerContext = m_server.GetProxy()->GetMessageContext(Processor::Test::EndpointIdentifier);
        ASSERT_TRUE(optServerContext);
        m_server.GetContext() = *optServerContext;

        m_client.GetProxy()->RegisterSilentEndpoint<InvokeContext::Test>(
            Processor::Test::EndpointIdentifier,
            Processor::Test::EndpointProtocol,
            Processor::Test::RemoteServerAddress,
            [this] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                auto const optMessage = Message::Platform::Parcel::GetBuilder()
                    .SetContext(m_client.GetContext())
                    .FromEncodedPack(std::get<std::string>(message))
                    .ValidatedBuild();
                EXPECT_TRUE(optMessage);

                Message::ValidationStatus status = optMessage->Validate();
                if (status != Message::ValidationStatus::Success) { return false; }
                m_optRequest = optMessage;
                return true;
            });

        auto const optClientContext = m_client.GetProxy()->GetMessageContext(Processor::Test::EndpointIdentifier);
        ASSERT_TRUE(optClientContext);
        m_client.GetContext() = *optClientContext;
    }

    void SetupLoopbackProxies()
    {
        m_server.GetProxy()->RegisterSilentEndpoint<InvokeContext::Test>(
            Processor::Test::EndpointIdentifier,
            Processor::Test::EndpointProtocol,
            Processor::Test::RemoteClientAddress,
            [&spProxy = m_client.GetProxy()] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                return spProxy->ScheduleReceive(
                    Processor::Test::EndpointIdentifier, std::get<std::string>(message));
            });
        
        auto const optServerContext = m_server.GetProxy()->GetMessageContext(Processor::Test::EndpointIdentifier);
        ASSERT_TRUE(optServerContext);
        m_server.GetContext() = *optServerContext;
        
        m_client.GetProxy()->RegisterSilentEndpoint<InvokeContext::Test>(
            Processor::Test::EndpointIdentifier,
            Processor::Test::EndpointProtocol,
            Processor::Test::RemoteServerAddress,
            [&spProxy = m_server.GetProxy()] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                return spProxy->ScheduleReceive(
                    Processor::Test::EndpointIdentifier, std::get<std::string>(message));
            });

        auto const optClientContext = m_client.GetProxy()->GetMessageContext(Processor::Test::EndpointIdentifier);
        ASSERT_TRUE(optClientContext);
        m_client.GetContext() = *optClientContext;
    }
    
    void SetupFailingProxies()
    {
        m_server.GetProxy()->RegisterSilentEndpoint<InvokeContext::Test>(
            Processor::Test::EndpointIdentifier,
            Processor::Test::EndpointProtocol,
            Processor::Test::RemoteClientAddress,
            [this] (auto const&, auto&&) -> bool { return false; });
        
        auto const optServerContext = m_server.GetProxy()->GetMessageContext(Processor::Test::EndpointIdentifier);
        ASSERT_TRUE(optServerContext);
        m_server.GetContext() = *optServerContext;

        m_client.GetProxy()->RegisterSilentEndpoint<InvokeContext::Test>(
            Processor::Test::EndpointIdentifier,
            Processor::Test::EndpointProtocol,
            Processor::Test::RemoteServerAddress,
            [this] (auto const&, auto&&) -> bool { return false; });

        auto const optClientContext = m_client.GetProxy()->GetMessageContext(Processor::Test::EndpointIdentifier);
        ASSERT_TRUE(optClientContext);
        m_client.GetContext() = *optClientContext;
    }

    local::ExchangeResources m_server{ test::ServerIdentifier, test::ClientIdentifier };
    local::ExchangeResources m_client{ test::ClientIdentifier, test::ServerIdentifier };

    Message::Platform::Parcel m_handshake;
    std::optional<Message::Platform::Parcel> m_optRequest;
    std::optional<Message::Platform::Parcel> m_optResponse;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, PrepareSuccessfulSecurityStrategyTest)
{
    SetupCaptureProxies();
    m_client.CreatePreparationSynchronizer(test::CaseType::Positive);

    // The processor stage should start out in the initialization stage. 
    EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Initialization);

    EXPECT_FALSE(m_client.GetObserver()->Notified());

    auto const [success, buffer] = m_client.GetProcessor()->Prepare();
    EXPECT_TRUE(success); // The processor should propagate the successful security strategy preparation. 
    
    auto const optMessage = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_client.GetContext())
        .FromEncodedPack(buffer)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage); // The processor should propagate the synchronization buffer through a platform message.

    EXPECT_EQ(optMessage->GetSource(), *test::ClientIdentifier);
    EXPECT_FALSE(optMessage->GetDestination()); // THe first handshake message will not have an explicit destination. 
    EXPECT_EQ(optMessage->GetDestinationType(), Message::Destination::Node);
    EXPECT_EQ(optMessage->GetType(), Message::Platform::ParcelType::Handshake);
    EXPECT_EQ(optMessage->GetPayload(), Processor::Test::Message);
 
    // After successfully preparing the exchange, the processor should now be in the synchronization stage. 
    EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Synchronization);
    EXPECT_FALSE(m_client.GetObserver()->Notified());

    // The processor should collect messages when in the synchronization stage. 
    EXPECT_TRUE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    // The test strategy doesn't indicate synchronization completion, so the observer should still not be called. 
    EXPECT_FALSE(m_client.GetObserver()->Notified());

    EXPECT_FALSE(m_optResponse);
    EXPECT_FALSE(m_optRequest);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, PrepareFailingSecurityStrategyTest)
{
    SetupCaptureProxies();
    m_client.CreatePreparationSynchronizer(test::CaseType::Negative);

    // The processor stage should start out in the initialization stage. 
    EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Initialization);

    EXPECT_FALSE(m_client.GetObserver()->Notified());

    auto const [success, buffer] = m_client.GetProcessor()->Prepare();
    EXPECT_FALSE(success); // The processor should propagate the failing security strategy preoperation. 
    EXPECT_TRUE(buffer.empty()); // The processor should not provide a synchronization buffer on error.

    // After failing to prepare the exchange, the processor should now be in the failure stage. 
    EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Failure);

    // The exchange observer should be notified of the failure. 
    EXPECT_TRUE(m_client.GetObserver()->Notified());
    EXPECT_EQ(m_client.GetObserver()->GetExchangeStatus(), ExchangeStatus::Failed);
    EXPECT_FALSE(m_client.GetObserver()->ExchangeSuccess());

    // The processor should not collect messages when in the failure stage. 
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    EXPECT_FALSE(m_optResponse);
    EXPECT_FALSE(m_optRequest);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, HandshakeInitiatorCloseTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Initiator));

    // The processor should collect messages when in the synchronization stage. 
    EXPECT_TRUE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    // The processor should respond immediately with the next synchronization message. 
    ASSERT_TRUE(m_optRequest);
    EXPECT_FALSE(m_optResponse);

    EXPECT_EQ(m_optRequest->GetSource(), *test::ClientIdentifier);
    EXPECT_EQ(m_optRequest->GetDestination(), *test::ServerIdentifier);
    EXPECT_EQ(m_optRequest->GetDestinationType(), Message::Destination::Node);
    EXPECT_EQ(m_optRequest->GetType(), Message::Platform::ParcelType::Handshake);
    EXPECT_EQ(m_optRequest->GetPayload(), Processor::Test::Message);

    // The observer should have been notified of the exchange success. 
    EXPECT_TRUE(m_client.GetObserver()->ExchangeSuccess());

    // Since the test security strategy requires only one synchronization message, the initiator exchange processor 
    // should use the connect protocol to continue application setup. 
    EXPECT_EQ(m_client.GetConnectProtocol()->Called(), std::size_t{ 1 });
    EXPECT_TRUE(m_client.GetConnectProtocol()->SentTo(test::ServerIdentifier));
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, HandshakeAcceptorCloseTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Acceptor));

    // The processor should collect messages when in the synchronization stage. 
    EXPECT_TRUE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    // The test strategy does not have any further handshake messages for acceptor role, so no responses should be sent. 
    EXPECT_FALSE(m_optResponse);
    EXPECT_FALSE(m_optRequest);

    // Since the test security strategy requires only one synchronization message, the initiator exchange processor 
    // should use the connect protocol to continue application setup. 
    EXPECT_TRUE(m_client.GetObserver()->ExchangeSuccess());

    // Currently, exchange processors with the acceptor role should not use the connect protocol. 
    EXPECT_EQ(m_client.GetConnectProtocol()->Called(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, HandshakeFailingStrategyTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Negative, Security::ExchangeRole::Acceptor));

    // The processor should collect messages when in the synchronization stage. 
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    // After failing to prepare the exchange, the processor should now be in the failure stage. 
    EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Failure);

    // The exchange observer should be notified of the failure. 
    EXPECT_TRUE(m_client.GetObserver()->Notified());
    EXPECT_EQ(m_client.GetObserver()->GetExchangeStatus(), ExchangeStatus::Failed);
    EXPECT_FALSE(m_client.GetObserver()->ExchangeSuccess());

    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, HandshakeUnexpectedDestinationTypeTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Acceptor));

    {
        auto const optHandshakeMessage = Message::Platform::Parcel::GetBuilder()
            .SetContext(m_client.GetContext())
            .SetSource(*test::ServerIdentifier)
            .MakeClusterMessage<InvokeContext::Test>()
            .SetPayload(Processor::Test::Message)
            .MakeHandshakeMessage()
            .ValidatedBuild();
        ASSERT_TRUE(optHandshakeMessage);

        // The processor should collect messages when in the synchronization stage. 
        EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), optHandshakeMessage->GetPack()));

        // After failing to prepare the exchange, the processor should now be in the failure stage. 
        EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Failure);
    }

    m_client.GetProcessor()->SetStage<InvokeContext::Test>(ExchangeProcessor::ProcessStage::Synchronization);

    {
        auto const optHandshakeMessage = Message::Platform::Parcel::GetBuilder()
            .SetContext(m_client.GetContext())
            .SetSource(*test::ServerIdentifier)
            .MakeNetworkMessage<InvokeContext::Test>()
            .SetPayload(Processor::Test::Message)
            .MakeHandshakeMessage()
            .ValidatedBuild();
        ASSERT_TRUE(optHandshakeMessage);

        // The processor should collect messages when in the synchronization stage. 
        EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), optHandshakeMessage->GetPack()));

        // After failing to prepare the exchange, the processor should now be in the failure stage. 
        EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Failure);
    }

    // The exchange observer should be notified of the failure. 
    EXPECT_TRUE(m_client.GetObserver()->Notified());
    EXPECT_EQ(m_client.GetObserver()->GetExchangeStatus(), ExchangeStatus::Failed);
    EXPECT_FALSE(m_client.GetObserver()->ExchangeSuccess());

    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, HandshakeUnexpectedDestinationTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Acceptor));

    auto const optHandshakeMessage = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_client.GetContext())
        .SetSource(*test::ServerIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .SetPayload(Processor::Test::Message)
        .MakeHandshakeMessage()
        .ValidatedBuild();
    ASSERT_TRUE(optHandshakeMessage);

    // The processor should collect messages when in the synchronization stage. 
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), optHandshakeMessage->GetPack()));

    // After failing to prepare the exchange, the processor should now be in the failure stage. 
    EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Failure);

    // The exchange observer should be notified of the failure. 
    EXPECT_TRUE(m_client.GetObserver()->Notified());
    EXPECT_EQ(m_client.GetObserver()->GetExchangeStatus(), ExchangeStatus::Failed);
    EXPECT_FALSE(m_client.GetObserver()->ExchangeSuccess());

    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, HandshakeFailingPeerTest)
{
    SetupFailingProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Initiator));

    // The processor should collect messages when in the synchronization stage. 
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    // After failing to prepare the exchange, the processor should now be in the failure stage. 
    EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Failure);

    // The exchange observer should be notified of the failure. 
    EXPECT_TRUE(m_client.GetObserver()->Notified());
    EXPECT_EQ(m_client.GetObserver()->GetExchangeStatus(), ExchangeStatus::Failed);
    EXPECT_FALSE(m_client.GetObserver()->ExchangeSuccess());

    EXPECT_EQ(m_client.GetConnectProtocol()->Called(), std::size_t{ 0 });

    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, HandshakeFailingConnectProtocolTest)
{
    SetupCaptureProxies();
    m_client.GetConnectProtocol()->FailSendRequests();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Initiator));

    // The processor should collect messages when in the synchronization stage. 
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    // After failing to prepare the exchange, the processor should now be in the failure stage. 
    EXPECT_EQ(m_client.GetProcessor()->GetProcessStage(), ExchangeProcessor::ProcessStage::Failure);

    // The exchange observer should be notified of the failure. 
    EXPECT_TRUE(m_client.GetObserver()->Notified());
    EXPECT_EQ(m_client.GetObserver()->GetExchangeStatus(), ExchangeStatus::Failed);
    EXPECT_FALSE(m_client.GetObserver()->ExchangeSuccess());

    EXPECT_EQ(m_client.GetConnectProtocol()->Called(), std::size_t{ 1 });

    EXPECT_TRUE(m_optRequest); // The final synchronization message is still successfully sent. 
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, CollectMalformedMessageBufferTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Acceptor));

    {
        auto const buffer = Message::Buffer{};
        EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), buffer));
    }

    {
        auto buffer = Message::Buffer{};
        buffer.resize(10'000);
        EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), buffer));
    }

    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, CollectMessageExpiredPeerTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Acceptor));
    
    auto context = m_client.GetContext();
    context.BindProxy<InvokeContext::Test>(std::weak_ptr<Peer::Proxy>{}); // Unbind the proxy.
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(context, m_handshake.GetPack()));

    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, CollectMessageUnexpectedStageTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Acceptor));

    m_client.GetProcessor()->SetStage<InvokeContext::Test>(ExchangeProcessor::ProcessStage::Initialization);
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    m_client.GetProcessor()->SetStage<InvokeContext::Test>(ExchangeProcessor::ProcessStage::Failure);
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), m_handshake.GetPack()));

    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, CollectPlatformParcelHeartbeatRequestTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Acceptor));
    
    auto const optHeartbeatRequest = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_client.GetContext())
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .MakeHeartbeatRequest()
        .ValidatedBuild();
    ASSERT_TRUE(optHeartbeatRequest);

    // Currently, heartbeat requests should be rejected by the exchange processor. 
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), optHeartbeatRequest->GetPack()));

    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, CollectPlatformParcelHeartbeatResponseTest)
{
    SetupCaptureProxies();
    EXPECT_TRUE(m_client.CreateBasicSynchronizer(test::CaseType::Positive, Security::ExchangeRole::Acceptor));

    auto const optHeartbeatResponse = Message::Platform::Parcel::GetBuilder()
        .SetContext(m_client.GetContext())
        .SetSource(*test::ClientIdentifier)
        .SetDestination(*test::ServerIdentifier)
        .MakeHeartbeatResponse()
        .ValidatedBuild();
    ASSERT_TRUE(optHeartbeatResponse);
    
    // Currently, heartbeat responses should be rejected by the exchange processor. 
    EXPECT_FALSE(m_client.GetProcessor()->CollectMessage(m_client.GetContext(), optHeartbeatResponse->GetPack()));
    
    EXPECT_FALSE(m_optRequest);
    EXPECT_FALSE(m_optResponse);
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ExchangeProcessorSuite, PackageSynchronizerTest)
{
    SetupLoopbackProxies();

    m_client.CreateProcessor(Security::ExchangeRole::Initiator);
    m_server.CreateProcessor(Security::ExchangeRole::Acceptor);

    // Prepare the client processor for the exchange. The processor will tell us if the exchange 
    // could be prepared and the request that needs to sent to the server. 
    auto clientPrepareResult = m_client.GetProcessor()->Prepare();
    EXPECT_TRUE(clientPrepareResult.first);
    EXPECT_GT(clientPrepareResult.second.size(), std::uint32_t{ 0 });

    // Prepare the server processor for the exchange. The processor will tell us if the preparation
    // succeeded. We do not expect to given an initial message to be sent given it is the acceptor. 
    auto const serverPrepareResult = m_server.GetProcessor()->Prepare();
    EXPECT_TRUE(serverPrepareResult.first);
    EXPECT_EQ(serverPrepareResult.second.size(), std::uint32_t{ 0 });

    // Start of the exchange by manually telling the client peer to send the exchange request.
    // This will cause the exchange transaction to occur of the stack. 
    EXPECT_TRUE(m_client.GetProxy()->ScheduleSend(
        Processor::Test::EndpointIdentifier, std::move(clientPrepareResult.second)));

    // We expect that the client observer was notified of a successful exchange, the connect 
    // protocol was called by client exchange, and the client peer sent the number of messages
    // required by the server. 
    EXPECT_TRUE(m_client.GetObserver()->ExchangeSuccess());
    EXPECT_EQ(m_client.GetConnectProtocol()->Called(), std::size_t{ 1 });
    EXPECT_TRUE(m_client.GetConnectProtocol()->SentTo(test::ServerIdentifier));
    EXPECT_GT(m_client.GetProxy()->GetSentCount(), std::size_t{ 0 });

    // We expect that the server observer was notified of a successful exchange, the connect 
    // protocol was called by server exchange, and the server peer sent the number of messages
    // required by the client. 
    EXPECT_TRUE(m_server.GetObserver()->ExchangeSuccess());
    EXPECT_EQ(m_server.GetConnectProtocol()->Called(), std::size_t{ 0 });
    EXPECT_FALSE(m_server.GetConnectProtocol()->SentTo(test::ClientIdentifier));
    EXPECT_GT(m_server.GetProxy()->GetSentCount(), std::size_t{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

local::ExchangeResources::ExchangeResources(
    Node::SharedIdentifier const& spSelf, Node::SharedIdentifier const& spTarget)
    : m_spRegistrar(std::make_shared<Scheduler::Registrar>())
    , m_spServiceProvider(std::make_shared<Node::ServiceProvider>())
    , m_spCipherService()
    , m_spTaskService(std::make_shared<Scheduler::TaskService>(m_spRegistrar))
    , m_spEventPublisher(std::make_shared<Event::Publisher>(m_spRegistrar))
    , m_spTrackingService()
    , m_spNodeState(std::make_shared<NodeState>(spSelf, Network::ProtocolSet{}))
    , m_context()
    , m_spProxy()
{
    auto const options = Configuration::Options::SupportedAlgorithms{
        {
            {
                Security::ConfidentialityLevel::High,
                Configuration::Options::Algorithms{ "high", { test::KeyAgreementName }, { test::CipherName }, { test::HashFunctionName } }
            }
        }
    };

    m_spCipherService = std::make_shared<Security::CipherService>(options);
    m_spServiceProvider->Register(m_spCipherService);

    m_spServiceProvider->Register(m_spTaskService);
    m_spServiceProvider->Register(m_spEventPublisher);
    m_spServiceProvider->Register(m_spNodeState);

    m_spTrackingService = std::make_shared<Awaitable::TrackingService>(m_spRegistrar);
    m_spServiceProvider->Register(m_spTrackingService);

    m_spConnectProtocol = std::make_shared<Processor::Test::ConnectProtocol>();
    m_spServiceProvider->Register<IConnectProtocol>(m_spConnectProtocol);

    m_spEventPublisher->SuspendSubscriptions();

    m_spProxy = Peer::Proxy::CreateInstance(*spTarget, m_spServiceProvider);
}

//----------------------------------------------------------------------------------------------------------------------

void local::ExchangeResources::CreateProcessor(Security::ExchangeRole role)
{
    m_upExchangeObserver = std::make_unique<Processor::Test::ExchangeObserver>();
    
    m_upExchangeProcessor = std::make_unique<ExchangeProcessor>(
        role,
        m_spServiceProvider,
        m_upExchangeObserver.get());

    m_spProxy->SetReceiver<InvokeContext::Test>(m_upExchangeProcessor.get());
}

//----------------------------------------------------------------------------------------------------------------------

void local::ExchangeResources::CreateTestProcessor(Security::ExchangeRole role, std::unique_ptr<ISynchronizer>&& upSynchronizer)
{
    m_upExchangeObserver = std::make_unique<Processor::Test::ExchangeObserver>();
    
    m_upExchangeProcessor = std::make_unique<ExchangeProcessor>(
        m_spServiceProvider,
        std::move(upSynchronizer),
        m_upExchangeObserver.get());

    m_spProxy->SetReceiver<InvokeContext::Test>(m_upExchangeProcessor.get());
}

//----------------------------------------------------------------------------------------------------------------------

void local::ExchangeResources::CreatePreparationSynchronizer(test::CaseType type)
{
    auto upSynchronizer = std::make_unique<test::PreparationSynchronizer>(type, Processor::Test::Message);
    CreateTestProcessor(Security::ExchangeRole::Initiator, std::move(upSynchronizer));
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ExchangeResources::CreateBasicSynchronizer(test::CaseType type, Security::ExchangeRole role)
{
    auto upSynchronizer = std::make_unique<test::BasicSynchronizer>(type, role, Processor::Test::Message);
    CreateTestProcessor(role, std::move(upSynchronizer));
    
    auto const [success, buffer] = m_upExchangeProcessor->Prepare();
    if (!success) { return false; }
    
    if (m_upExchangeProcessor->GetProcessStage() != ExchangeProcessor::ProcessStage::Synchronization) { return false; }

    if (m_upExchangeObserver->Notified()) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
