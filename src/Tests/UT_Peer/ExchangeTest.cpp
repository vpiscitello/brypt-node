
//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/ProxyStore.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/ConnectProtocol.hpp"
#include "Interfaces/ResolutionService.hpp"
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

    Scheduler::Registrar& GetScheduler() const;
    Node::Identifier const& GetIdentifier() const;
    Peer::Test::ConnectProtocol const& GetConnectProtocol() const;
    Peer::ProxyStore& GetProxyStore() const;

private:
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<Peer::Test::ConnectProtocol> m_spConnectProtocol;
    std::shared_ptr<Peer::Test::MessageProcessor> m_spMessageProcessor;
    
    std::shared_ptr<Peer::ProxyStore> m_spProxyStore;
};

//----------------------------------------------------------------------------------------------------------------------

TEST(PeerExchangeSuite, PQNISTL3ExchangeSetupTest)
{
    local::ExchangeResources server;
    local::ExchangeResources client;

    std::shared_ptr<Peer::Proxy> spServerProxy; // The server peer is associated with the client's manager.
    std::shared_ptr<Peer::Proxy> spClientProxy; // The client peer is associated with the server's manager.

    // Simulate an endpoint delcaring that it is attempting to resolve a peer at a given uri. 
    auto const optRequest = client.GetProxyStore().DeclareResolvingPeer(Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(optRequest);
    EXPECT_GT(optRequest->size(), std::size_t{ 0 });
    EXPECT_EQ(client.GetProxyStore().ActiveCount(), std::size_t{ 0 });

    // Simulate the server receiving the connection request. 
    spClientProxy = server.GetProxyStore().LinkPeer(client.GetIdentifier(), Peer::Test::RemoteClientAddress);
    ASSERT_TRUE(spClientProxy);
    EXPECT_FALSE(spClientProxy->IsAuthorized());
    EXPECT_FALSE(spClientProxy->IsFlagged());
    EXPECT_EQ(server.GetProxyStore().ObservedCount(), std::size_t(1));

    // Simulate the server's endpoint registering itself to the given client peer. 
    spClientProxy->RegisterEndpoint(
        Peer::Test::EndpointIdentifier,
        Peer::Test::EndpointProtocol,
        Peer::Test::RemoteClientAddress,
        [&] (auto const&, auto&& message) -> bool {
            auto const optContext = spServerProxy->GetMessageContext(Peer::Test::EndpointIdentifier);
            if (!optContext) { return false; }

            EXPECT_TRUE(spServerProxy->ScheduleReceive(
                optContext->GetEndpointIdentifier(), std::get<std::string>(message)));
            return true;
        });

    // In practice the client would receive a response from the server before linking a peer. 
    // However, we need to create a peer to properly handle the exchange on the stack. 
    spServerProxy = client.GetProxyStore().LinkPeer(server.GetIdentifier(), Peer::Test::RemoteServerAddress);
    ASSERT_TRUE(spServerProxy);
    EXPECT_FALSE(spServerProxy->IsAuthorized());
    EXPECT_FALSE(spServerProxy->IsFlagged());
    EXPECT_EQ(client.GetProxyStore().ObservedCount(), std::size_t{ 1 });

    // Simulate the clients's endpoint registering itself to the given server peer. 
    spServerProxy->RegisterEndpoint(
        Peer::Test::EndpointIdentifier,
        Peer::Test::EndpointProtocol,
        Peer::Test::RemoteServerAddress,
        [&] (auto const&, auto&& message) -> bool {
            auto const optContext = spClientProxy->GetMessageContext(Peer::Test::EndpointIdentifier);
            if (!optContext) { return false; }

            EXPECT_TRUE(spClientProxy->ScheduleReceive(
                optContext->GetEndpointIdentifier(), std::get<std::string>(message)));
            return true;
        });

    auto const optContext = spClientProxy->GetMessageContext(Peer::Test::EndpointIdentifier);
    ASSERT_TRUE(optContext);

    // Cause the key exchange setup by the peer manager to occur on the stack. 
    EXPECT_TRUE(spClientProxy->ScheduleReceive(optContext->GetEndpointIdentifier(), *optRequest));

    // Verify the results of the key exchange
    EXPECT_TRUE(client.GetConnectProtocol().CalledOnce());
    EXPECT_TRUE(spClientProxy->IsAuthorized());
    EXPECT_TRUE(spServerProxy->IsAuthorized());

    // The peer manager should have notified the scheduler in order to clean up the resolved peer's resolvers. 
    EXPECT_EQ(client.GetScheduler().Execute(), std::size_t{ 1 });
    EXPECT_EQ(server.GetScheduler().Execute(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

local::ExchangeResources::ExchangeResources()   
    : m_spRegistrar(std::make_shared<Scheduler::Registrar>())
    , m_spServiceProvider(std::make_shared<Node::ServiceProvider>())
    , m_spEventPublisher(std::make_shared<Event::Publisher>(m_spRegistrar))
    , m_spNodeState(std::make_shared<NodeState>(
        std::make_shared<Node::Identifier>(Node::GenerateIdentifier()), Network::ProtocolSet{}))
    , m_spTrackingService(std::make_shared<Awaitable::TrackingService>(m_spRegistrar))
    , m_spConnectProtocol(std::make_shared<Peer::Test::ConnectProtocol>())
    , m_spMessageProcessor(std::make_shared<Peer::Test::MessageProcessor>())
    , m_spProxyStore()
{
    m_spServiceProvider->Register(m_spEventPublisher);
    m_spServiceProvider->Register(m_spNodeState);
    m_spServiceProvider->Register(m_spTrackingService);
    m_spServiceProvider->Register<IConnectProtocol>(m_spConnectProtocol);
    m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);

    m_spProxyStore = std::make_shared<Peer::ProxyStore>(
        Security::Strategy::PQNISTL3, m_spRegistrar, m_spServiceProvider);
    m_spServiceProvider->Register<IResolutionService>(m_spProxyStore);
    
    EXPECT_TRUE(m_spRegistrar->Initialize());

    m_spEventPublisher->SuspendSubscriptions();
}

//----------------------------------------------------------------------------------------------------------------------

Scheduler::Registrar& local::ExchangeResources::GetScheduler() const
{
    if (!m_spRegistrar) {
        throw std::logic_error("Peer exchange test resources have not be properly initialied!");
    }
    return *m_spRegistrar;
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

Peer::Test::ConnectProtocol const& local::ExchangeResources::GetConnectProtocol() const
{
    if (!m_spConnectProtocol) {
        throw std::logic_error("Peer exchange test resources have not be properly initialied!");
    }
    return *m_spConnectProtocol;
}

//----------------------------------------------------------------------------------------------------------------------

Peer::ProxyStore& local::ExchangeResources::GetProxyStore() const
{
    if (!m_spProxyStore) {
        throw std::logic_error("Peer exchange test resources have not be properly initialied!");
    }
    return *m_spProxyStore;
}

//----------------------------------------------------------------------------------------------------------------------
