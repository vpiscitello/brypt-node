//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Action.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/ProxyStore.hpp"
#include "Components/Route/Connect.hpp"
#include "Components/Route/MessageHandler.hpp"
#include "Components/Route/Router.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Components/State/NodeState.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class ConnectResources;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

auto const ClientIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());
auto const ServerIdentifier = std::make_shared<Node::Identifier>(Node::GenerateIdentifier());

constexpr std::string_view NetworkInterface = "lo";
constexpr std::string_view ServerBinding = "*:35216";
constexpr std::string_view ClientBinding = "*:35217";

constexpr auto RuntimeOptions = Configuration::Options::Runtime
{ 
    .context = RuntimeContext::Foreground,
    .verbosity = spdlog::level::debug,
    .useInteractiveConsole = false,
    .useBootstraps = false,
    .useFilepathDeduction = false
};

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::ConnectResources
{
public:
    ConnectResources() = default;
    ~ConnectResources() = default;

    void Initialize(Node::SharedIdentifier const& spSelf, Node::SharedIdentifier const& spTarget, std::string_view binding);

    [[nodiscard]] std::shared_ptr<Node::ServiceProvider> const& GetServiceProvider() const { return m_spServiceProvider; }
    [[nodiscard]] std::shared_ptr<Awaitable::TrackingService> const& GetTrackingService() const { return m_spTrackingService; }
    [[nodiscard]] std::shared_ptr<BootstrapService> const& GetBootstrapService() const { return m_spBootstrapService; }   
    [[nodiscard]] Configuration::Options::Endpoints const& GetEndpointConfiguration() const { return m_configuration; }
    [[nodiscard]] std::shared_ptr<Network::Manager> const& GetNetworkManager() const { return m_spNetworkManager; }   
    [[nodiscard]] std::shared_ptr<Peer::ProxyStore> const& GetProxyStore() const { return m_spProxyStore; }
    [[nodiscard]] Message::Context& GetContext() { return m_context; }
    [[nodiscard]] std::shared_ptr<Peer::Proxy> const& GetProxy() const { return m_spProxy; }
    [[nodiscard]] std::vector<std::shared_ptr<Peer::Proxy>> const& GetPeers() const { return m_peers; }
    [[nodiscard]] std::vector<Message::Application::Parcel> const& GetEchoes() const { return m_echoes; }

    [[nodiscard]] Route::Fundamental::Connect::DiscoveryProtocol& GetDiscoveryProtocol() const { return *m_spDiscoveryProtocol; }
    [[nodiscard]] Route::Fundamental::Connect::DiscoveryHandler& GetDiscoveryHandler() const { return *m_upDiscoveryHandler; }
    [[nodiscard]] bool IsConstructionSuccessful() const { return m_isConstructionSuccessful; }

    [[nodiscard]] Network::Manager::SharedEndpoint FindEndpoint(Network::Protocol protocol) const
    {
        return m_spNetworkManager->GetEndpoint(protocol);
    }

    [[nodiscard]] bool AddGeneratedEndpoints(std::vector<Network::BindingAddress> const& bindings);

private:
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<BootstrapService> m_spBootstrapService;
    Configuration::Options::Endpoints m_configuration;
    std::shared_ptr<Network::Manager> m_spNetworkManager;
    std::shared_ptr<Peer::ProxyStore> m_spProxyStore;
    Message::Context m_context;
    std::shared_ptr<Peer::Proxy> m_spProxy;
    std::vector<std::shared_ptr<Peer::Proxy>> m_peers;
    std::vector<Message::Application::Parcel> m_echoes;

    std::shared_ptr<Route::Fundamental::Connect::DiscoveryProtocol> m_spDiscoveryProtocol;
    std::unique_ptr<Route::Fundamental::Connect::DiscoveryHandler> m_upDiscoveryHandler;
    bool m_isConstructionSuccessful;
};

//----------------------------------------------------------------------------------------------------------------------

class ConnectHandlerSuite : public testing::Test
{
protected:
    void SetUp() override
    {
        m_server.Initialize(test::ServerIdentifier, test::ClientIdentifier, test::ServerBinding);
        m_client.Initialize(test::ClientIdentifier, test::ServerIdentifier, test::ClientBinding);

        ASSERT_TRUE(m_server.IsConstructionSuccessful());
        ASSERT_TRUE(m_client.IsConstructionSuccessful());

        m_server.GetProxy()->RegisterSilentEndpoint<InvokeContext::Test>(
            Route::Test::EndpointIdentifier, Route::Test::EndpointProtocol, Route::Test::RemoteClientAddress,
            [this] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                auto const optMessage = Message::Application::Parcel::GetBuilder()
                    .SetContext(m_server.GetContext())
                    .FromEncodedPack(std::get<std::string>(message))
                    .ValidatedBuild();
                EXPECT_TRUE(optMessage);

                Message::ValidationStatus status = optMessage->Validate();
                if (status != Message::ValidationStatus::Success) { return false; }
                m_optResponse = optMessage;
                return true;
            });

        auto const optServerContext = m_server.GetProxy()->GetMessageContext(Route::Test::EndpointIdentifier);
        ASSERT_TRUE(optServerContext);
        m_server.GetContext() = *optServerContext;

        m_client.GetProxy()->RegisterSilentEndpoint<InvokeContext::Test>(
            Route::Test::EndpointIdentifier, Route::Test::EndpointProtocol, Route::Test::RemoteServerAddress,
            [this] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
                auto const optMessage = Message::Application::Parcel::GetBuilder()
                    .SetContext(m_client.GetContext())
                    .FromEncodedPack(std::get<std::string>(message))
                    .ValidatedBuild();
                EXPECT_TRUE(optMessage);

                Message::ValidationStatus status = optMessage->Validate();
                if (status != Message::ValidationStatus::Success) { return false; }
                m_optRequest = optMessage;
                return true;
            });
        
        auto const optClientContext = m_client.GetProxy()->GetMessageContext(Route::Test::EndpointIdentifier);
        ASSERT_TRUE(optClientContext);
        m_client.GetContext() = *optClientContext;

        ASSERT_TRUE(m_client.GetDiscoveryHandler().OnFetchServices(m_client.GetServiceProvider()));
        ASSERT_TRUE(m_server.GetDiscoveryHandler().OnFetchServices(m_server.GetServiceProvider()));
    }

    local::ConnectResources m_server;
    local::ConnectResources m_client;

    std::optional<Message::Application::Parcel> m_optRequest;
    std::optional<Message::Application::Parcel> m_optResponse;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectHandlerSuite, DiscoveryProtocolRequestTest)
{
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 0 });
    EXPECT_TRUE(m_client.GetDiscoveryProtocol().SendRequest(m_client.GetProxy(), m_client.GetContext()));
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 1 });

    ASSERT_TRUE(m_optRequest);
    EXPECT_EQ(m_optRequest->GetSource(), *test::ClientIdentifier);
    EXPECT_EQ(m_optRequest->GetDestination(), *test::ServerIdentifier);
    EXPECT_EQ(m_optRequest->GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
    EXPECT_FALSE(m_optRequest->GetPayload().IsEmpty());

    auto const optRequestExtension = m_optRequest->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optRequestExtension);
    EXPECT_EQ(optRequestExtension->get().GetBinding(), Message::Extension::Awaitable::Binding::Request);
    ASSERT_NE(optRequestExtension->get().GetTracker(), Awaitable::TrackerKey{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectHandlerSuite, DiscoveryHandlerSingleEntrypointTest)
{
    auto const spServerEndpoint = std::dynamic_pointer_cast<Route::Test::StandardEndpoint>(
        m_server.FindEndpoint(Network::Protocol::Test));
    ASSERT_TRUE(spServerEndpoint);

    auto const spClientEndpoint = std::dynamic_pointer_cast<Route::Test::StandardEndpoint>(
        m_client.FindEndpoint(Network::Protocol::Test));
    ASSERT_TRUE(spClientEndpoint);

    EXPECT_TRUE(m_client.GetDiscoveryProtocol().SendRequest(m_client.GetProxy(), m_client.GetContext()));
    ASSERT_TRUE(m_optRequest);

    EXPECT_EQ(spServerEndpoint->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(spServerEndpoint->GetConnected(), BootstrapService::BootstrapCache{ });
    EXPECT_FALSE(spServerEndpoint->GetPeerIdentifier());
    EXPECT_EQ(m_server.GetBootstrapService()->BootstrapCount(), std::size_t{ 0 });
    EXPECT_EQ(m_server.GetEchoes().size(), std::size_t{ 0 });

    Peer::Action::Next next{ m_server.GetProxy(), *m_optRequest, m_server.GetServiceProvider() };
    EXPECT_TRUE(m_server.GetDiscoveryHandler().OnMessage(*m_optRequest, next));

    // The discovery request should cause the receiving node to add the initiator's bootstrap cache 
    // whether or not it can actaullly connect to them. 
    EXPECT_EQ(m_server.GetBootstrapService()->BootstrapCount(), std::size_t{ 1 });

    // The discovery request handler should not schedule a connection if the peer only has one 
    // entrypoint of the same protocol used to make the discovery request.
    EXPECT_EQ(spServerEndpoint->GetScheduled(), std::uint32_t{ 0 });

    // The request handler should echo the discovery request to it's connected peers such that they 
    // can connect to the new peer. 
    EXPECT_EQ(m_server.GetPeers().size(), m_server.GetEchoes().size());

    // Verify the information stored in an echo sent out by the server. 
    {
        ASSERT_FALSE(m_server.GetEchoes().empty());

        auto const& echo = m_server.GetEchoes().front();
        EXPECT_EQ(echo.GetSource(), *test::ServerIdentifier);
        EXPECT_EQ(echo.GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
        EXPECT_FALSE(echo.GetPayload().IsEmpty());
        EXPECT_TRUE(echo.GetExtension<Message::Extension::Echo>());
    }

    ASSERT_TRUE(m_optResponse);
    EXPECT_EQ(m_optResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResponse->GetDestination(), *test::ClientIdentifier);
    EXPECT_EQ(m_optResponse->GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
    EXPECT_FALSE(m_optResponse->GetPayload().IsEmpty());

    auto const optResponseStatus = m_optResponse->GetExtension<Message::Extension::Status>();
    EXPECT_TRUE(optResponseStatus);
    EXPECT_EQ(optResponseStatus->get().GetCode(), Message::Extension::Status::Accepted);
    
    auto const optResponseAwaitable = m_optResponse->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optResponseAwaitable);
    EXPECT_EQ(optResponseAwaitable->get().GetBinding(), Message::Extension::Awaitable::Binding::Response);

    auto const optRequestAwaitable = m_optRequest->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optRequestAwaitable);
    ASSERT_EQ(optResponseAwaitable->get().GetTracker(), optRequestAwaitable->get().GetTracker());

    EXPECT_EQ(spClientEndpoint->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(spClientEndpoint->GetConnected(), BootstrapService::BootstrapCache{ });
    EXPECT_FALSE(spClientEndpoint->GetPeerIdentifier());
    EXPECT_EQ(m_client.GetEchoes().size(), std::size_t{ 0 });

    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 0 });
    EXPECT_TRUE(m_client.GetTrackingService()->Process(std::move(*m_optResponse)));

    // The discovery response should cause the awaiting request to be fulfilled and executable on the next 
    // processing cycle. 
    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 1 });
    EXPECT_EQ(m_client.GetTrackingService()->Execute(), std::size_t{ 1 });

    // The discovery response should cause the receiving node to add the acceptors's bootstrap cache 
    // whether or not it can actaullly connect to them. 
    EXPECT_EQ(m_client.GetBootstrapService()->BootstrapCount(), std::size_t{ 1 });

    // The discovery response handler should not schedule a connection if the peer only has one 
    // entrypoint of the same protocol used to make the discovery request.
    EXPECT_EQ(spServerEndpoint->GetScheduled(), std::uint32_t{ 0 });

    // The response handler should echo the discovery request to it's connected peers such that they 
    // can connect to the new peer. 
    EXPECT_EQ(m_client.GetPeers().size(), m_client.GetEchoes().size());

    // Verify the information stored in an echo sent out by the client. 
    {
        ASSERT_FALSE(m_client.GetEchoes().empty());

        auto const& echo = m_client.GetEchoes().front();
        EXPECT_EQ(echo.GetSource(), *test::ClientIdentifier);
        EXPECT_EQ(echo.GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
        EXPECT_FALSE(echo.GetPayload().IsEmpty());
        EXPECT_TRUE(echo.GetExtension<Message::Extension::Echo>());
    }

    // Verify an echo can be processed by the handler. Note: We are piping an echo created by the resource
    // set back into the resources own handler. In actuality a node won't echo to itself and if it did, 
    // the message should be filtered out by the message processor. 
    {
        m_optResponse.reset(); // We need to reset to verify if an echo is handled, a response is not sent back.
        spServerEndpoint->ClearPeerIdentifier();
        EXPECT_FALSE(spServerEndpoint->GetPeerIdentifier());

        auto const initialEchoes = m_server.GetEchoes().size();

        auto const& echo = m_server.GetEchoes().front();
        Peer::Action::Next next{ m_server.GetProxy(), echo, m_server.GetServiceProvider() };
        EXPECT_TRUE(m_server.GetDiscoveryHandler().OnMessage(echo, next));

        // We expect the echo to result in scheduling another connection, but no additional echoes. 
        EXPECT_EQ(spServerEndpoint->GetScheduled(), std::size_t{ 0 });
        EXPECT_EQ(m_server.GetEchoes().size(), initialEchoes);
        EXPECT_FALSE(m_optResponse);
    }

    // Verify client view echo. 
    {
        spServerEndpoint->ClearPeerIdentifier();
        EXPECT_FALSE(spServerEndpoint->GetPeerIdentifier());

        auto const initialEchoes = m_client.GetEchoes().size();

        auto const& echo = m_client.GetEchoes().front();
        Peer::Action::Next next{ m_client.GetProxy(), echo, m_client.GetServiceProvider() };
        EXPECT_TRUE(m_client.GetDiscoveryHandler().OnMessage(echo, next));

        // We expect the echo to result in scheduling another connection, but no additional echoes. 
        EXPECT_EQ(spClientEndpoint->GetScheduled(), std::size_t{ 0 });
        EXPECT_EQ(m_client.GetEchoes().size(), initialEchoes);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectHandlerSuite, DiscoveryHandlerMultipleEntrypointsTest)
{
    // Setup an additional endpoint for the connection acceptor. 
    Network::BindingAddress const serverTcpBindingAddress{ Network::Protocol::TCP, test::ServerBinding, "lo" };
    Network::RemoteAddress const serverTcpRemoteAddress{ 
        Network::Protocol::TCP, serverTcpBindingAddress.GetUri(), true, Network::RemoteAddress::Origin::User};
    ASSERT_TRUE(m_server.AddGeneratedEndpoints({ serverTcpBindingAddress }));
    
    auto const spServerTestEndpoint = std::dynamic_pointer_cast<Route::Test::StandardEndpoint>(
        m_server.FindEndpoint(Network::Protocol::Test));
    ASSERT_TRUE(spServerTestEndpoint);

    auto const spServerTcpEndpoint = std::dynamic_pointer_cast<Route::Test::StandardEndpoint>(
        m_server.FindEndpoint(Network::Protocol::TCP));
    ASSERT_TRUE(spServerTcpEndpoint);
    
    // Setup an additional endpoint for the connection initiator.
    Network::BindingAddress const clientTcpBindingAddress{ Network::Protocol::TCP, test::ClientBinding, "lo" };
    Network::RemoteAddress const clientTcpRemoteAddress{ 
        Network::Protocol::TCP, clientTcpBindingAddress.GetUri(), true, Network::RemoteAddress::Origin::User};
    ASSERT_TRUE(m_client.AddGeneratedEndpoints({ clientTcpBindingAddress }));

    auto const spClientTestEndpoint = std::dynamic_pointer_cast<Route::Test::StandardEndpoint>(
        m_client.FindEndpoint(Network::Protocol::Test));
    ASSERT_TRUE(spClientTestEndpoint);

    auto const spClientTcpEndpoint = std::dynamic_pointer_cast<Route::Test::StandardEndpoint>(
        m_client.FindEndpoint(Network::Protocol::TCP));
    ASSERT_TRUE(spClientTcpEndpoint);
    

    EXPECT_TRUE(m_client.GetDiscoveryProtocol().SendRequest(m_client.GetProxy(), m_client.GetContext()));
    ASSERT_TRUE(m_optRequest);

    EXPECT_EQ(spServerTestEndpoint->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(spServerTcpEndpoint->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(spServerTcpEndpoint->GetConnected(), BootstrapService::BootstrapCache{ });
    EXPECT_FALSE(spServerTcpEndpoint->GetPeerIdentifier());
    EXPECT_EQ(m_server.GetBootstrapService()->BootstrapCount(), std::size_t{ 0 });
    EXPECT_EQ(m_server.GetEchoes().size(), std::size_t{ 0 });
    EXPECT_TRUE(m_server.GetProxy()->IsRemoteConnected(Route::Test::RemoteClientAddress));
    EXPECT_FALSE(m_server.GetProxy()->IsRemoteAssociated(clientTcpRemoteAddress));

    Peer::Action::Next next{ m_server.GetProxy(), *m_optRequest, m_server.GetServiceProvider() };
    EXPECT_TRUE(m_server.GetDiscoveryHandler().OnMessage(*m_optRequest, next));

    // The discovery request should cause the receiving node to add the initiator's bootstrap cache 
    // whether or not it can actaullly connect to them. 
    EXPECT_EQ(m_server.GetBootstrapService()->BootstrapCount(), std::size_t{ 2 });

    // The discovery request should cause the receiving node to connect to the initiator's server addresses. 
    EXPECT_EQ(spServerTestEndpoint->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(spServerTcpEndpoint->GetScheduled(), std::uint32_t{ 1 });
    EXPECT_TRUE(spServerTcpEndpoint->GetConnected().contains(clientTcpRemoteAddress));
    EXPECT_EQ(*spServerTcpEndpoint->GetPeerIdentifier(), *test::ClientIdentifier);
    EXPECT_TRUE(m_server.GetProxy()->IsRemoteAssociated(clientTcpRemoteAddress));
    EXPECT_FALSE(m_server.GetProxy()->IsRemoteConnected(clientTcpRemoteAddress));

    // The request handler should echo the discovery request to it's connected peers such that they 
    // can connect to the new peer. 
    EXPECT_EQ(m_server.GetPeers().size(), m_server.GetEchoes().size());

    // Verify the information stored in an echo sent out by the server. 
    {
        ASSERT_FALSE(m_server.GetEchoes().empty());
     
        auto const& echo = m_server.GetEchoes().front();
        EXPECT_EQ(echo.GetSource(), *test::ServerIdentifier);
        EXPECT_EQ(echo.GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
        EXPECT_FALSE(echo.GetPayload().IsEmpty());
        EXPECT_TRUE(echo.GetExtension<Message::Extension::Echo>());
    }

    ASSERT_TRUE(m_optResponse);
    EXPECT_EQ(m_optResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResponse->GetDestination(), *test::ClientIdentifier);
    EXPECT_EQ(m_optResponse->GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
    EXPECT_FALSE(m_optResponse->GetPayload().IsEmpty());

    auto const optResponseStatus = m_optResponse->GetExtension<Message::Extension::Status>();
    EXPECT_TRUE(optResponseStatus);
    EXPECT_EQ(optResponseStatus->get().GetCode(), Message::Extension::Status::Accepted);
    
    auto const optResponseAwaitable = m_optResponse->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optResponseAwaitable);
    EXPECT_EQ(optResponseAwaitable->get().GetBinding(), Message::Extension::Awaitable::Binding::Response);

    auto const optRequestAwaitable = m_optRequest->GetExtension<Message::Extension::Awaitable>();
    EXPECT_TRUE(optRequestAwaitable);
    ASSERT_EQ(optResponseAwaitable->get().GetTracker(), optRequestAwaitable->get().GetTracker());

    EXPECT_EQ(spClientTestEndpoint->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(spClientTcpEndpoint->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(spClientTcpEndpoint->GetConnected(), BootstrapService::BootstrapCache{ });
    EXPECT_FALSE(spClientTcpEndpoint->GetPeerIdentifier());
    EXPECT_EQ(m_client.GetEchoes().size(), std::size_t{ 0 });
    EXPECT_TRUE(m_client.GetProxy()->IsRemoteConnected(Route::Test::RemoteServerAddress));
    EXPECT_FALSE(m_client.GetProxy()->IsRemoteAssociated(serverTcpRemoteAddress));

    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 0 });
    EXPECT_TRUE(m_client.GetTrackingService()->Process(std::move(*m_optResponse)));

    // The discovery response should cause the awaiting request to be fulfilled and executable on the next 
    // processing cycle. 
    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 1 });
    EXPECT_EQ(m_client.GetTrackingService()->Execute(), std::size_t{ 1 });

    // The discovery response should cause the receiving node to add the acceptors's bootstrap cache 
    // whether or not it can actaullly connect to them. 
    EXPECT_EQ(m_client.GetBootstrapService()->BootstrapCount(), std::size_t{ 2 });

    // The discovery response should cause the receiving node to connect to the acceptors's server addresses. 
    EXPECT_EQ(spClientTestEndpoint->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(spClientTcpEndpoint->GetScheduled(), std::uint32_t{ 1 });
    EXPECT_TRUE(spClientTcpEndpoint->GetConnected().contains(serverTcpRemoteAddress));
    EXPECT_EQ(*spClientTcpEndpoint->GetPeerIdentifier(), *test::ServerIdentifier);
    EXPECT_TRUE(m_client.GetProxy()->IsRemoteAssociated(serverTcpRemoteAddress));
    EXPECT_FALSE(m_client.GetProxy()->IsRemoteConnected(serverTcpRemoteAddress));

    // The response handler should echo the discovery request to it's connected peers such that they 
    // can connect to the new peer. 
    EXPECT_EQ(m_client.GetPeers().size(), m_client.GetEchoes().size());

    // Verify the information stored in an echo sent out by the client. 
    {
        ASSERT_FALSE(m_client.GetEchoes().empty());
     
        auto const& echo = m_client.GetEchoes().front();
        EXPECT_EQ(echo.GetSource(), *test::ClientIdentifier);
        EXPECT_EQ(echo.GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
        EXPECT_FALSE(echo.GetPayload().IsEmpty());
        EXPECT_TRUE(echo.GetExtension<Message::Extension::Echo>());
    }

    // Verify an echo can be processed by the handler. Note: We are piping an echo created by the resource
    // set back into the resources own handler. In actuality a node won't echo to itself and if it did, 
    // the message should be filtered out by the message processor. 
    {
        m_optResponse.reset(); // We need to reset to verify if an echo is handled, a response is not sent back.
        spServerTcpEndpoint->ClearPeerIdentifier();
        EXPECT_FALSE(spServerTcpEndpoint->GetPeerIdentifier());

        auto const initialConnectionsScheduled = spClientTcpEndpoint->GetScheduled();
        auto const initialEchoes = m_server.GetEchoes().size();

        auto const& echo = m_server.GetEchoes().front();
        Peer::Action::Next next{ m_server.GetProxy(), echo, m_server.GetServiceProvider() };
        EXPECT_TRUE(m_server.GetDiscoveryHandler().OnMessage(echo, next));

        // We expect the echo to result in scheduling another connection, but no additional echoes. 
        EXPECT_EQ(spServerTcpEndpoint->GetScheduled(), initialConnectionsScheduled + 1);
        EXPECT_EQ(m_server.GetEchoes().size(), initialEchoes);
        EXPECT_FALSE(m_optResponse);

        // Even though the server is the source of the echo, the identifier supplied for the connection
        // attempt should peer that of peer joining the network.
        EXPECT_EQ(*spServerTcpEndpoint->GetPeerIdentifier(), *test::ClientIdentifier);
    }

    // Verify client view echo. 
    {
        spServerTcpEndpoint->ClearPeerIdentifier();
        EXPECT_FALSE(spServerTcpEndpoint->GetPeerIdentifier());

        auto const initialConnectionsScheduled = spClientTcpEndpoint->GetScheduled();
        auto const initialEchoes = m_client.GetEchoes().size();

        auto const& echo = m_client.GetEchoes().front();
        Peer::Action::Next next{ m_client.GetProxy(), echo, m_client.GetServiceProvider() };
        EXPECT_TRUE(m_client.GetDiscoveryHandler().OnMessage(echo, next));

        // We expect the echo to result in scheduling another connection, but no additional echoes. 
        EXPECT_EQ(spClientTcpEndpoint->GetScheduled(), initialConnectionsScheduled + 1);
        EXPECT_EQ(m_client.GetEchoes().size(), initialEchoes);

        // Even though the client is the source of the echo, the identifier supplied for the connection
        // attempt should peer that the client joined to.
        EXPECT_EQ(*spClientTcpEndpoint->GetPeerIdentifier(), *test::ServerIdentifier);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void local::ConnectResources::Initialize(
    Node::SharedIdentifier const& spSelf, Node::SharedIdentifier const& spTarget, std::string_view binding)
{
    m_isConstructionSuccessful = false;

    m_spRegistrar = std::make_shared<Scheduler::Registrar>();
    m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

    m_spTaskService = std::make_shared<Scheduler::TaskService>(m_spRegistrar);
    m_spServiceProvider->Register(m_spTaskService);

    m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
    m_spServiceProvider->Register(m_spEventPublisher);
    
    m_spTrackingService = std::make_shared<Awaitable::TrackingService>(m_spRegistrar);
    m_spServiceProvider->Register(m_spTrackingService);
    
    m_spNodeState = std::make_shared<NodeState>(spSelf, Network::ProtocolSet{});
    m_spServiceProvider->Register(m_spNodeState);
    
    m_spBootstrapService = std::make_shared<BootstrapService>();
    m_spServiceProvider->Register(m_spBootstrapService);

    m_spNetworkManager = std::make_shared<Network::Manager>(test::RuntimeOptions.context, m_spServiceProvider);
    m_spServiceProvider->Register(m_spNetworkManager);

    auto const options = Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(
        test::NetworkInterface, binding);
    m_configuration.emplace_back(options);

    m_spNetworkManager->RegisterEndpoint<InvokeContext::Test>(
        options, std::make_shared<Route::Test::StandardEndpoint>(Network::Endpoint::Properties{ options }));

    m_spDiscoveryProtocol = std::make_shared<Route::Fundamental::Connect::DiscoveryProtocol>();
    m_spServiceProvider->Register<IConnectProtocol>(m_spDiscoveryProtocol);

    m_upDiscoveryHandler = std::make_unique<Route::Fundamental::Connect::DiscoveryHandler>();

    m_spProxyStore = std::make_shared<Peer::ProxyStore>(
        Security::Strategy::PQNISTL3, m_spRegistrar, m_spServiceProvider);
    m_spServiceProvider->Register(m_spProxyStore);

    m_isConstructionSuccessful = m_spDiscoveryProtocol->CompileRequest(m_spServiceProvider);

    m_spEventPublisher->SuspendSubscriptions();

    m_spProxy = Peer::Proxy::CreateInstance(*spTarget, m_spServiceProvider);
 
    std::random_device device;
    std::mt19937 engine(device());
    std::uniform_int_distribution distribution(35217, 35255);

    std::ranges::generate_n(std::back_insert_iterator(m_peers), 5, [&] () {
        std::ostringstream oss;
        oss << "127.0.0.1:" << distribution(engine);
        Network::RemoteAddress address{ Network::Protocol::TCP, oss.str(), true };
        
        auto const spProxy = m_spProxyStore->LinkPeer(Node::Identifier{ Node::GenerateIdentifier() }, address);
        
        spProxy->SetAuthorization<InvokeContext::Test>(Security::State::Authorized);

        spProxy->RegisterSilentEndpoint<InvokeContext::Test>(
            Route::Test::EndpointIdentifier, Route::Test::EndpointProtocol, address,
            [&, spProxy] ([[maybe_unused]] auto const& destination, auto&& message) -> bool {
            
            auto const optContext = spProxy->GetMessageContext(Route::Test::EndpointIdentifier);
            if (!optContext) { return {}; }

            auto optMessage = Message::Application::Parcel::GetBuilder()
                .SetContext(*optContext)
                .FromEncodedPack(std::get<std::string>(message))
                .ValidatedBuild();
            if (!optMessage || optMessage->Validate() != Message::ValidationStatus::Success) { return false; }

            m_echoes.emplace_back(std::move(*optMessage));
            
            return true;
        });
        
        return spProxy;
    });
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ConnectResources::AddGeneratedEndpoints(std::vector<Network::BindingAddress> const& bindings)
{
    m_isConstructionSuccessful = false;

    for (auto const& binding : bindings) {
        auto const options = Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(binding);
        m_configuration.emplace_back(options);

        m_spNetworkManager->RegisterEndpoint<InvokeContext::Test>(
            options, std::make_shared<Route::Test::StandardEndpoint>(Network::Endpoint::Properties{ options }));
    }

    // Regenerate the discovery handlers such that they can recompile the discovery payloads. 
    m_upDiscoveryHandler = std::make_unique<Route::Fundamental::Connect::DiscoveryHandler>();
    if (!m_upDiscoveryHandler->OnFetchServices(m_spServiceProvider)) { return false; }

    if (!m_spDiscoveryProtocol->CompileRequest(m_spServiceProvider)) { return false; }

    m_isConstructionSuccessful = true;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
