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

class StandardEndpoint;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::ConnectResources
{
public:
    ConnectResources(Node::SharedIdentifier const& spSelf, Node::SharedIdentifier const& spTarget, std::string_view binding);
    
    [[nodiscard]] std::shared_ptr<Node::ServiceProvider> const& GetServiceProvider() const { return m_spServiceProvider; }
    [[nodiscard]] std::shared_ptr<Awaitable::TrackingService> const& GetTrackingService() const { return m_spTrackingService; }
    [[nodiscard]] std::shared_ptr<BootstrapService> const& GetBootstrapService() const { return m_spBootstrapService; }   
    [[nodiscard]] Configuration::Options::Endpoints const& GetEndpointConfiguration() const { return m_endpoints; }
    [[nodiscard]] std::shared_ptr<Network::Manager> const& GetNetworkManager() const { return m_spNetworkManager; }   
    [[nodiscard]] std::shared_ptr<test::StandardEndpoint> const& GetServerEndpoint() const { return m_spServerEndpoint; }   
    [[nodiscard]] std::shared_ptr<test::StandardEndpoint> const& GetClientEndpoint() const { return m_spClientEndpoint; }   
    [[nodiscard]] Message::Context const& GetContext() const { return m_context; }
    [[nodiscard]] std::shared_ptr<Peer::Proxy> const& GetProxy() const { return m_spProxy; }   

private:
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<BootstrapService> m_spBootstrapService;
    Configuration::Options::Endpoints m_endpoints;
    std::shared_ptr<Network::Manager> m_spNetworkManager;
    std::shared_ptr<test::StandardEndpoint> m_spServerEndpoint;
    std::shared_ptr<test::StandardEndpoint> m_spClientEndpoint;
    Message::Context m_context;
    std::shared_ptr<Peer::Proxy> m_spProxy;
};

//----------------------------------------------------------------------------------------------------------------------


class test::StandardEndpoint : public Network::IEndpoint
{
public:
    explicit StandardEndpoint(Network::Endpoint::Properties const& properties)
        : IEndpoint(properties)
        , m_scheduled(0)
        , m_connected()
    {
    }

    std::uint32_t const& GetScheduled() const { return m_scheduled; }
    BootstrapService::BootstrapCache const& GetConnected() const { return m_connected; }

    // IEndpoint {
    [[nodiscard]] virtual Network::Protocol GetProtocol() const override { return Network::Protocol::Test; }
    [[nodiscard]] virtual std::string_view GetScheme() const override { return Network::TestScheme; }
    [[nodiscard]] virtual Network::BindingAddress GetBinding() const override { return m_binding; }

    virtual void Startup() override {}
	[[nodiscard]] virtual bool Shutdown() override { return true; }
    [[nodiscard]] virtual bool IsActive() const override { return true; }

    [[nodiscard]] virtual bool ScheduleBind(Network::BindingAddress const&) override { return true; }
    [[nodiscard]] virtual bool ScheduleConnect(Network::RemoteAddress const& address) override
    {
        return ScheduleConnect(Network::RemoteAddress{ address }, nullptr);
    }
    [[nodiscard]] virtual bool ScheduleConnect(Network::RemoteAddress&& address) override
    {
        return ScheduleConnect(std::move(address), nullptr);
    }
    [[nodiscard]] virtual bool ScheduleConnect(Network::RemoteAddress&& address, Node::SharedIdentifier const&) override
    {
        ++m_scheduled;
        m_connected.emplace(address);
        return true;
    }
    [[nodiscard]] virtual bool ScheduleDisconnect(Network::RemoteAddress const&) override { return false; }
    [[nodiscard]] virtual bool ScheduleDisconnect(Network::RemoteAddress&&) override { return false; }
	[[nodiscard]] virtual bool ScheduleSend(Node::Identifier const&, std::string&&) override { return true; }
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const&, Message::ShareablePack const&) override { return true; }
    [[nodiscard]] virtual bool ScheduleSend(Node::Identifier const&, Network::MessageVariant&&) override { return true; }
    // } IEndpoint

private:
    std::uint32_t m_scheduled;
    BootstrapService::BootstrapCache m_connected;
};

//----------------------------------------------------------------------------------------------------------------------

class ConnectHandlerSuite : public testing::Test
{
protected:
    void SetUp() override
    {
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

        m_upConnectProtocol = std::make_unique<Route::Fundamental::Connect::DiscoveryProtocol>(
            m_client.GetEndpointConfiguration(), m_client.GetServiceProvider());

        m_upConnectHandler = std::make_unique<Route::Fundamental::Connect::DiscoveryHandler>();
        ASSERT_TRUE(m_upConnectHandler->OnFetchServices(m_server.GetServiceProvider()));
    }

    local::ConnectResources m_server{ test::ServerIdentifier, test::ClientIdentifier, test::ServerBinding };
    local::ConnectResources m_client{ test::ClientIdentifier, test::ServerIdentifier, test::ClientBinding };
    
    std::unique_ptr<Route::Fundamental::Connect::DiscoveryProtocol> m_upConnectProtocol;
    std::unique_ptr<Route::Fundamental::Connect::DiscoveryHandler> m_upConnectHandler;

    std::optional<Message::Application::Parcel> m_optRequest;
    std::optional<Message::Application::Parcel> m_optResponse;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectHandlerSuite, DiscoveryProtocolRequestTest)
{
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 0 });
    EXPECT_TRUE(m_upConnectProtocol->SendRequest(m_client.GetProxy(), m_client.GetContext()));
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 1 });

    ASSERT_TRUE(m_optRequest);
    EXPECT_EQ(m_optRequest->GetSource(), *test::ClientIdentifier);
    EXPECT_EQ(m_optRequest->GetDestination(), *test::ServerIdentifier);
    EXPECT_EQ(m_optRequest->GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
    EXPECT_FALSE(m_optRequest->GetPayload().IsEmpty());

    auto const optRequestExtension = m_optRequest->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optRequestExtension);
    EXPECT_EQ(optRequestExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Request);
    ASSERT_NE(optRequestExtension->get().GetTracker(), Awaitable::TrackerKey{ 0 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(ConnectHandlerSuite, DiscoveryHandlerValidMessageTest)
{
    EXPECT_TRUE(m_upConnectProtocol->SendRequest(m_client.GetProxy(), m_client.GetContext()));
    ASSERT_TRUE(m_optRequest);

    EXPECT_EQ(m_server.GetBootstrapService()->BootstrapCount(), std::size_t{ 0 });
    EXPECT_EQ(m_server.GetClientEndpoint()->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(m_server.GetClientEndpoint()->GetConnected(), BootstrapService::BootstrapCache{ });

    auto const unknownRemote = Network::RemoteAddress::CreateTestAddress<InvokeContext::Test>("*:35218", true);
    m_server.GetBootstrapService()->InsertBootstrap(unknownRemote);
    EXPECT_EQ(m_server.GetBootstrapService()->BootstrapCount(), std::size_t{ 1 });
    
    Peer::Action::Next next{ m_server.GetProxy(), *m_optRequest, m_server.GetServiceProvider() };
    EXPECT_TRUE(m_upConnectHandler->OnMessage(*m_optRequest, next));

    // The discovery request should cause the receiving node to add the initiator's bootstrap cache 
    // whether or not it can actaullly connect to them. 
    EXPECT_EQ(m_server.GetBootstrapService()->BootstrapCount(), std::size_t{ 2 });

    // The discovery request should cause the receiving node to connect to the initiator's server addresses. 
    EXPECT_EQ(m_server.GetClientEndpoint()->GetScheduled(), std::uint32_t{ 1 });
    EXPECT_TRUE(m_server.GetClientEndpoint()->GetConnected().contains(Route::Test::RemoteClientAddress));

    ASSERT_TRUE(m_optResponse);
    EXPECT_EQ(m_optResponse->GetSource(), *test::ServerIdentifier);
    EXPECT_EQ(m_optResponse->GetDestination(), *test::ClientIdentifier);
    EXPECT_EQ(m_optResponse->GetRoute(), Route::Fundamental::Connect::DiscoveryHandler::Path);
    EXPECT_FALSE(m_optResponse->GetPayload().IsEmpty());

    auto const optResponseExtension = m_optResponse->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optResponseExtension);
    EXPECT_EQ(optResponseExtension->get().GetBinding(), Message::Application::Extension::Awaitable::Binding::Response);

    auto const optRequestExtension = m_optRequest->GetExtension<Message::Application::Extension::Awaitable>();
    EXPECT_TRUE(optRequestExtension);
    ASSERT_EQ(optResponseExtension->get().GetTracker(), optRequestExtension->get().GetTracker());

    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 0 });
    EXPECT_EQ(m_client.GetClientEndpoint()->GetScheduled(), std::uint32_t{ 0 });
    EXPECT_EQ(m_client.GetClientEndpoint()->GetConnected(), BootstrapService::BootstrapCache{ });

    EXPECT_TRUE(m_client.GetTrackingService()->Process(std::move(*m_optResponse)));

    // The discovery response should cause the awaiting request to be fulfilled and executable on the next 
    // processing cycle. 
    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 1 });
    EXPECT_EQ(m_client.GetTrackingService()->Execute(), std::size_t{ 1 });

    // The discovery response should cause the initiator to connect to the provided endpoints. The response
    // will contain the initiator's own address (it is up to the endpoint to determine if the address is reflective
    // or not).
    EXPECT_EQ(m_client.GetClientEndpoint()->GetScheduled(), std::uint32_t{ 2 });
    EXPECT_TRUE(m_client.GetClientEndpoint()->GetConnected().contains(Route::Test::RemoteClientAddress));
    EXPECT_TRUE(m_client.GetClientEndpoint()->GetConnected().contains(unknownRemote));
}

//----------------------------------------------------------------------------------------------------------------------

local::ConnectResources::ConnectResources(
    Node::SharedIdentifier const& spSelf, Node::SharedIdentifier const& spTarget, std::string_view binding)
    : m_spRegistrar(std::make_shared<Scheduler::Registrar>())
    , m_spServiceProvider(std::make_shared<Node::ServiceProvider>())
    , m_spTaskService(std::make_shared<Scheduler::TaskService>(m_spRegistrar))
    , m_spEventPublisher(std::make_shared<Event::Publisher>(m_spRegistrar))
    , m_spTrackingService()
    , m_spNodeState(std::make_shared<NodeState>(spSelf, Network::ProtocolSet{}))
    , m_spBootstrapService(std::make_shared<BootstrapService>())
    , m_spNetworkManager()
    , m_context(Route::Test::GenerateMessageContext())
    , m_spProxy()
{
    m_spServiceProvider->Register(m_spTaskService);
    m_spServiceProvider->Register(m_spEventPublisher);
    m_spServiceProvider->Register(m_spNodeState);
    m_spServiceProvider->Register(m_spBootstrapService);

    m_spTrackingService = std::make_shared<Awaitable::TrackingService>(m_spRegistrar, m_spServiceProvider);
    m_spServiceProvider->Register(m_spTrackingService);
    
    auto options = Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(
        test::NetworkInterface, binding);
    m_endpoints.emplace_back(std::move(options));

    m_spNetworkManager = std::make_shared<Network::Manager>(test::RuntimeOptions.context, m_spServiceProvider);
    m_spServiceProvider->Register(m_spNetworkManager);

    m_spServerEndpoint = std::make_shared<test::StandardEndpoint>(
        Network::Endpoint::Properties{ Network::Operation::Server, options });
    m_spNetworkManager->RegisterEndpoint<InvokeContext::Test>(options, m_spServerEndpoint);

    m_spClientEndpoint = std::make_shared<test::StandardEndpoint>(
        Network::Endpoint::Properties{ Network::Operation::Client, options });
    m_spNetworkManager->RegisterEndpoint<InvokeContext::Test>(options, m_spClientEndpoint);

    m_spEventPublisher->SuspendSubscriptions();

    m_spProxy = Peer::Proxy::CreateInstance(*spTarget, m_spServiceProvider);
}

//----------------------------------------------------------------------------------------------------------------------
