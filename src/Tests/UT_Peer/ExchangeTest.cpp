
//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Manager.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/PeerMediator.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <random>
#include <ranges>
#include <string>
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

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::ExchangeResources
{
public:
    ExchangeResources();
    Node::Identifier const& GetIdentifier() const;
    Message::Context const& GetContext() const;
    Peer::Test::ConnectProtocol const& GetConnectProtocol() const;
    Peer::Manager& GetManager() const;

private:
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Peer::Test::ConnectProtocol> m_spConnectProtocol;
    std::shared_ptr<Peer::Test::MessageProcessor> m_spMessageProcessor;
    Message::Context m_context;
    
    std::shared_ptr<Peer::Manager> m_spManager;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerExchangeSuite, PQNISTL3ExchangeSetupTest)
{
    auto const client = local::ExchangeResources();
    auto const server = local::ExchangeResources();

    std::shared_ptr<Peer::Proxy> spServerProxy; // The server peer is associated with the client's manager.
    std::shared_ptr<Peer::Proxy> spClientProxy; // The client peer is associated with the server's manager.

    // Simulate an endpoint delcaring that it is attempting to resolve a peer at a given uri. 
    auto const optRequest = client.GetManager().DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });
    EXPECT_EQ(client.GetManager().ActiveCount(), std::size_t{ 0 });

    // Simulate the server receiving the connection request. 
    spClientProxy = server.GetManager().LinkPeer(client.GetIdentifier(), Peer::Test::RemoteClientAddress);
    EXPECT_FALSE(spClientProxy->IsAuthorized());
    EXPECT_FALSE(spClientProxy->IsFlagged());
    EXPECT_EQ(server.GetManager().ObservedCount(), std::size_t(1));

    // Simulate the server's endpoint registering itself to the given client peer. 
    spClientProxy->RegisterEndpoint(
        client.GetContext().GetEndpointIdentifier(), client.GetContext().GetEndpointProtocol(), Peer::Test::RemoteClientAddress,
        [&spServerProxy, context = server.GetContext()] (auto const&, auto&& message) -> bool {
            EXPECT_TRUE(spServerProxy->ScheduleReceive(context.GetEndpointIdentifier(), std::get<std::string>(message)));
            return true;
        });

    // In practice the client would receive a response from the server before linking a peer. 
    // However, we need to create a peer to properly handle the exchange on the stack. 
    spServerProxy = client.GetManager().LinkPeer(server.GetIdentifier(), Peer::Test::RemoteServerAddress);
    EXPECT_FALSE(spServerProxy->IsAuthorized());
    EXPECT_FALSE(spServerProxy->IsFlagged());
    EXPECT_EQ(client.GetManager().ObservedCount(), std::size_t{ 1 });

    // Simulate the clients's endpoint registering itself to the given server peer. 
    spServerProxy->RegisterEndpoint(
        server.GetContext().GetEndpointIdentifier(), server.GetContext().GetEndpointProtocol(), Peer::Test::RemoteServerAddress,
        [&spClientProxy, context = client.GetContext()] (auto const&, auto&& message) -> bool {
            EXPECT_TRUE(spClientProxy->ScheduleReceive(context.GetEndpointIdentifier(), std::get<std::string>(message)));
            return true;
        });

    // Cause the key exchange setup by the peer manager to occur on the stack. 
    EXPECT_TRUE(spClientProxy->ScheduleReceive(client.GetContext().GetEndpointIdentifier(), *optRequest));

    // Verify the results of the key exchange
    EXPECT_TRUE(client.GetConnectProtocol().CalledOnce());
    EXPECT_TRUE(spClientProxy->IsAuthorized());
    EXPECT_TRUE(spServerProxy->IsAuthorized());
}

//----------------------------------------------------------------------------------------------------------------------

local::ExchangeResources::ExchangeResources()   
    : m_spRegistrar(std::make_shared<Scheduler::Registrar>())
    , m_spServiceProvider(std::make_shared<Node::ServiceProvider>())
    , m_spEventPublisher(std::make_shared<Event::Publisher>(m_spRegistrar))
    , m_spNodeState(std::make_shared<NodeState>(
        std::make_shared<Node::Identifier>(Node::GenerateIdentifier()), Network::ProtocolSet{}))
    , m_spConnectProtocol(std::make_shared<Peer::Test::ConnectProtocol>())
    , m_spMessageProcessor(std::make_shared<Peer::Test::MessageProcessor>())
    , m_context(
        Network::Endpoint::IdentifierGenerator::Instance().Generate(),
        Network::Protocol::TCP)
    , m_spManager()
{
    m_spServiceProvider->Register(m_spEventPublisher);
    m_spServiceProvider->Register(m_spNodeState);
    m_spServiceProvider->Register<IConnectProtocol>(m_spConnectProtocol);
    m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);

    m_spManager = std::make_shared<Peer::Manager>(Security::Strategy::PQNISTL3, m_spServiceProvider);
    m_spServiceProvider->Register<IPeerMediator>(m_spManager);
    
    m_spEventPublisher->SuspendSubscriptions();
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& local::ExchangeResources::GetIdentifier() const
{
    if (!m_spNodeState || !m_spNodeState->GetNodeIdentifier()) {
        throw std::logic_error("Peer exchange test resources have not be properly initialied!");
    }
    return *m_spNodeState->GetNodeIdentifier();
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context const& local::ExchangeResources::GetContext() const { return m_context; }

//----------------------------------------------------------------------------------------------------------------------

Peer::Test::ConnectProtocol const& local::ExchangeResources::GetConnectProtocol() const
{
    if (!m_spConnectProtocol) {
        throw std::logic_error("Peer exchange test resources have not be properly initialied!");
    }
    return *m_spConnectProtocol;
}

//----------------------------------------------------------------------------------------------------------------------

Peer::Manager& local::ExchangeResources::GetManager() const
{
    if (!m_spManager) {
        throw std::logic_error("Peer exchange test resources have not be properly initialied!");
    }
    return *m_spManager;
}

//----------------------------------------------------------------------------------------------------------------------
