//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Message/PlatformMessage.hpp"
#include "Components/Message/MessageContext.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Resolver.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Security/CipherService.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/ResolutionService.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

void CloseExchange(Peer::Resolver* pResolver, ExchangeStatus status);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

std::string const KeyAgreementName = "kem-kyber768";
std::string const CipherName = "aes-256-ctr";
std::string const HashFunctionName = "sha384";

auto const ClientIdentifier = Node::Identifier{ Node::GenerateIdentifier() };
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
    
//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class PeerResolverSuite : public testing::Test
{
protected:
    void SetUp() override
    {
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

        m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
        m_spServiceProvider->Register(m_spEventPublisher);

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

        m_spNodeState = std::make_shared<NodeState>(test::ServerIdentifier, Network::ProtocolSet{});
        m_spServiceProvider->Register(m_spNodeState);

        m_spConnectProtocol = std::make_shared<Peer::Test::ConnectProtocol>();
        m_spServiceProvider->Register<IConnectProtocol>(m_spConnectProtocol);

        m_spMessageProcessor = std::make_shared<Peer::Test::MessageProcessor>();
        m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);
        
        m_spResolutionService = std::make_shared<Peer::Test::ResolutionService>();
        m_spServiceProvider->Register<IResolutionService>(m_spResolutionService);

        m_spProxy = Peer::Proxy::CreateInstance(test::ClientIdentifier, m_spServiceProvider);
    }

    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Security::CipherService> m_spCipherService;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Peer::Test::ConnectProtocol> m_spConnectProtocol;
    std::shared_ptr<Peer::Test::MessageProcessor> m_spMessageProcessor;
    std::shared_ptr<Peer::Test::ResolutionService> m_spResolutionService;
    std::shared_ptr<Peer::Proxy> m_spProxy;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerResolverSuite, ExchangeProcessorLifecycleTest)
{
    {
        auto upResolver = std::make_unique<Peer::Resolver>();
        ASSERT_TRUE(upResolver->SetupCustomExchange<InvokeContext::Test>(m_spServiceProvider, std::make_unique<Peer::Test::Synchronizer>()));
        ASSERT_TRUE(m_spProxy->AttachResolver(std::move(upResolver)));
    }

    m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>( 
        Peer::Test::EndpointIdentifier,
        Peer::Test::EndpointProtocol,
        Peer::Test::RemoteClientAddress);

    auto const optMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(Peer::Test::HandshakePayload)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();
    EXPECT_TRUE(m_spProxy->ScheduleReceive(Peer::Test::EndpointIdentifier, pack));

    // Verify the node can't forward a message through the receiver, because it has been unset by the mediator. 
    m_spProxy->DetachResolver();

    EXPECT_FALSE(m_spProxy->ScheduleReceive(Peer::Test::EndpointIdentifier, pack));
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerResolverSuite, SuccessfulExchangeTest)
{
    Peer::Resolver* pCapturedResolver = nullptr;
    {
        auto upResolver = std::make_unique<Peer::Resolver>();
        pCapturedResolver = upResolver.get();
        ASSERT_TRUE(upResolver->SetupCustomExchange<InvokeContext::Test>(m_spServiceProvider, std::make_unique<Peer::Test::Synchronizer>()));
        ASSERT_TRUE(m_spProxy->AttachResolver(std::move(upResolver)));
    }

    m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>( 
        Peer::Test::EndpointIdentifier,
        Peer::Test::EndpointProtocol,
        Peer::Test::RemoteClientAddress);

    auto const optMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(Peer::Test::HandshakePayload)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();
    EXPECT_TRUE(m_spProxy->ScheduleReceive(Peer::Test::EndpointIdentifier, pack));

    // Verify the receiver is swapped to the authorized processor when resolver is notified of a successful exchange. 
    local::CloseExchange(pCapturedResolver, ExchangeStatus::Success);
    EXPECT_EQ(m_spProxy->GetAuthorization(), Security::State::Authorized);
    EXPECT_TRUE(m_spProxy->ScheduleReceive(Peer::Test::EndpointIdentifier, pack));
    EXPECT_EQ(m_spMessageProcessor->GetCollectedPack(), pack);  // Verify the stub message sink received the message
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(PeerResolverSuite, FailedExchangeTest)
{
    Peer::Resolver* pCapturedResolver = nullptr;
    {
        auto upResolver = std::make_unique<Peer::Resolver>();
        pCapturedResolver = upResolver.get();
        ASSERT_TRUE(upResolver->SetupCustomExchange<InvokeContext::Test>(m_spServiceProvider, std::make_unique<Peer::Test::Synchronizer>()));
        ASSERT_TRUE(m_spProxy->AttachResolver(std::move(upResolver)));
    }

    m_spProxy->RegisterSilentEndpoint<InvokeContext::Test>( 
        Peer::Test::EndpointIdentifier,
        Peer::Test::EndpointProtocol,
        Peer::Test::RemoteClientAddress);
    
    auto const optMessage = Message::Platform::Parcel::GetBuilder()
        .SetSource(*test::ServerIdentifier)
        .MakeHandshakeMessage()
        .SetPayload(Peer::Test::HandshakePayload)
        .ValidatedBuild();
    ASSERT_TRUE(optMessage);

    std::string const pack = optMessage->GetPack();
    EXPECT_TRUE(m_spProxy->ScheduleReceive(Peer::Test::EndpointIdentifier, pack));

    // Verify the peer receiver is dropped when the resolver has been notified of a failed exchange. 
    local::CloseExchange(pCapturedResolver, ExchangeStatus::Failed);
    EXPECT_EQ(m_spProxy->GetAuthorization(), Security::State::Unauthorized);
    EXPECT_FALSE(m_spProxy->ScheduleReceive(Peer::Test::EndpointIdentifier, pack));
}

//----------------------------------------------------------------------------------------------------------------------

void local::CloseExchange(Peer::Resolver* pResolver, ExchangeStatus status)
{
    pResolver->OnExchangeClose(status); // Note: the resolver will be destroyed by the peer after this call. 
    pResolver = nullptr; // Ensure the captured resolver is not dangling. 
}

//----------------------------------------------------------------------------------------------------------------------
