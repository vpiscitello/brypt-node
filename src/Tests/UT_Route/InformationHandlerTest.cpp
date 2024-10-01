//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Core/ServiceProvider.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Message/ApplicationMessage.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Action.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Route/Information.hpp"
#include "Components/Route/MessageHandler.hpp"
#include "Components/Route/Router.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Components/State/CoordinatorState.hpp"
#include "Components/State/NetworkState.hpp"
#include "Components/State/NodeState.hpp"
#include "Utilities/CallbackIteration.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json.hpp>
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

class InformationResources;

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

constexpr std::size_t PeerCount = 5;

constexpr auto RuntimeOptions = Configuration::Options::Runtime
{ 
    .context = RuntimeContext::Foreground,
    .verbosity = spdlog::level::debug,
    .useInteractiveConsole = false,
    .useBootstraps = false,
    .useFilepathDeduction = false
};

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
//----------------------------------------------------------------------------------------------------------------------
namespace symbols {
//----------------------------------------------------------------------------------------------------------------------

auto const Cluster = "cluster";
auto const Coordinator = "coordinator";
auto const Designation = "designation";
auto const NeighborCount = "neighbor_count";
auto const Protocols = "protocols";
auto const UpdateTimestamp = "update_timestamp";

//----------------------------------------------------------------------------------------------------------------------
} // symbols namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::InformationResources
{
public:
    InformationResources(Node::SharedIdentifier const& spSelf, Node::SharedIdentifier const& spTarget, std::string_view binding);
    
    [[nodiscard]] std::shared_ptr<Node::ServiceProvider> const& GetServiceProvider() const { return m_spServiceProvider; }
    [[nodiscard]] std::shared_ptr<Awaitable::TrackingService> const& GetTrackingService() const { return m_spTrackingService; }
    [[nodiscard]] std::shared_ptr<CoordinatorState> const& GetCoordinatorState() const { return m_spCoordinatorState; }   
    [[nodiscard]] std::shared_ptr<NetworkState> const& GetNetworkState() const { return m_spNetworkState; }   
    [[nodiscard]] std::shared_ptr<NodeState> const& GetNodeState() const { return m_spNodeState; }   
    [[nodiscard]] std::shared_ptr<Network::Manager> const& GetNetworkManager() const { return m_spNetworkManager; }   
    [[nodiscard]] std::shared_ptr<Route::Test::PeerCache> const& GetPeerCache() const { return m_spPeerCache; }   
    [[nodiscard]] Message::Context& GetContext() { return m_context; }
    [[nodiscard]] std::shared_ptr<Peer::Proxy> const& GetProxy() const { return m_spProxy; }   

private:
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<Awaitable::TrackingService> m_spTrackingService;
    std::shared_ptr<CoordinatorState> m_spCoordinatorState;
    std::shared_ptr<NetworkState> m_spNetworkState;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<BootstrapService> m_spBootstrapService;
    Configuration::Options::Endpoints m_endpoints;
    std::shared_ptr<Network::Manager> m_spNetworkManager;
    std::shared_ptr<Route::Test::StandardEndpoint> m_spEndpoint;
    std::shared_ptr<Route::Test::PeerCache> m_spPeerCache;
    Message::Context m_context;
    std::shared_ptr<Peer::Proxy> m_spProxy;
};

//----------------------------------------------------------------------------------------------------------------------

class InformationHandlerSuite : public testing::Test
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

        m_upNodeHandler = std::make_unique<Route::Fundamental::Information::NodeHandler>();
        ASSERT_TRUE(m_upNodeHandler->OnFetchServices(m_server.GetServiceProvider()));

        m_upFetchNodeHandler = std::make_unique<Route::Fundamental::Information::FetchNodeHandler>();
        ASSERT_TRUE(m_upFetchNodeHandler->OnFetchServices(m_server.GetServiceProvider()));
    }

    local::InformationResources m_server{ test::ServerIdentifier, test::ClientIdentifier, test::ServerBinding };
    local::InformationResources m_client{ test::ClientIdentifier, test::ServerIdentifier, test::ServerBinding };
    
    std::unique_ptr<Route::Fundamental::Information::NodeHandler> m_upNodeHandler;
    std::unique_ptr<Route::Fundamental::Information::FetchNodeHandler> m_upFetchNodeHandler;

    std::optional<Message::Application::Parcel> m_optRequest;
    std::optional<Message::Application::Parcel> m_optResponse;
};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(InformationHandlerSuite, NodeHandlerTest)
{
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 0 });

    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_client.GetContext())
        .SetSource(*test::ClientIdentifier)
        .SetRoute(Route::Fundamental::Information::NodeHandler::Path);

    auto const wpProxy = std::weak_ptr<Peer::Proxy>{ m_client.GetProxy() };
    auto const optTrackerKey = m_client.GetProxy()->Request(builder,
        [&] (Peer::Action::Response const& response) {
            auto const spProxy = wpProxy.lock();
            ASSERT_TRUE(spProxy);

            boost::json::error_code error;
            auto const json = boost::json::parse(response.GetPayload().GetStringView());
            ASSERT_FALSE(error);
            ASSERT_TRUE(json.is_object());

            auto const& object = json.get_object();

            {
                auto const& field = object.at(symbols::Cluster);
                ASSERT_TRUE(field.is_int64());

                auto const& value = field.get_int64();
                EXPECT_EQ(value, m_server.GetNodeState()->GetCluster());
            }

            {
                auto const& field = object.at(symbols::Designation);
                ASSERT_TRUE(field.is_string());

                auto const& value = field.get_string();
                EXPECT_EQ(value, NodeUtils::GetDesignation(m_server.GetNodeState()->GetOperation()));
            }

            {
                auto const& field = object.at(symbols::NeighborCount);
                ASSERT_TRUE(field.is_int64());

                auto const& value = field.get_int64();
                EXPECT_EQ(value, static_cast<std::int64_t>(m_server.GetPeerCache()->ActiveCount()));
            }

            {
                auto const& field = object.at(symbols::Protocols);
                ASSERT_TRUE(field.is_array());

                auto const& value = field.get_array();
                std::vector<std::string> protocols;
                std::ranges::transform(value, std::back_inserter(protocols), [&] (boost::json::value const& entry) -> std::string {
                    EXPECT_TRUE(entry.is_string());
                    return entry.as_string().c_str();
                });
                EXPECT_EQ(protocols, std::vector<std::string>{ std::string{ Network::TestScheme } });
            }

            {
                auto const& field = object.at(symbols::UpdateTimestamp);
                ASSERT_TRUE(field.is_int64());

                auto const& value = field.get_int64();
                EXPECT_EQ(value, m_server.GetNetworkState()->GetUpdatedTimepoint().time_since_epoch().count());
            }
        },
        [&] (Peer::Action::Response const&) { ASSERT_FALSE(true); });
    EXPECT_TRUE(optTrackerKey);
    ASSERT_TRUE(m_optRequest);
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 1 });

    Peer::Action::Next next{ m_server.GetProxy(), *m_optRequest, m_server.GetServiceProvider() };
    EXPECT_TRUE(m_upNodeHandler->OnMessage(*m_optRequest, next));

    ASSERT_TRUE(m_optResponse);
    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 0 });
    EXPECT_TRUE(m_client.GetTrackingService()->Process(std::move(*m_optResponse)));

    // The node information response should cause the awaiting request to be fulfilled and executable 
    // on the next processing cycle. 
    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 1 });
    EXPECT_EQ(m_client.GetTrackingService()->Execute(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(InformationHandlerSuite, FetchNodeHandlerTest)
{
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 0 });

    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(m_client.GetContext())
        .SetSource(*test::ClientIdentifier)
        .SetRoute(Route::Fundamental::Information::FetchNodeHandler::Path);

    auto const wpProxy = std::weak_ptr<Peer::Proxy>{ m_client.GetProxy() };
    auto const optTrackerKey = m_client.GetProxy()->Request(builder,
        [&] (Peer::Action::Response const& response) {
            auto const spProxy = wpProxy.lock();
            ASSERT_TRUE(spProxy);


            boost::json::error_code error;
            auto const responseEntriesJson = boost::json::parse(response.GetPayload().GetStringView(), error);
            ASSERT_FALSE(error);
            ASSERT_TRUE(responseEntriesJson.is_object());

            std::string const& serverIdentifier = static_cast<std::string const&>(*test::ServerIdentifier);
            bool hasFoundServerResponse = false;
            for (auto const& [identifier, data]:responseEntriesJson.get_object()) {
                ASSERT_TRUE(data.is_string());

                if (identifier == serverIdentifier) {
                    //auto const json = std::string_view{ reinterpret_cast<char const*>(entry.data.data()), entry.data.size() };

                    boost::json::error_code error;
                    auto const json = boost::json::parse(response.GetPayload().GetStringView());
                    ASSERT_FALSE(error);
                    ASSERT_TRUE(json.is_object());

                    auto const& object = json.get_object();

                    {
                        auto const& field = object.at(symbols::Cluster);
                        ASSERT_TRUE(field.is_int64());

                        auto const& value = field.get_int64();
                        EXPECT_EQ(value, m_server.GetNodeState()->GetCluster());
                    }

                    {
                        auto const& field = object.at(symbols::Designation);
                        ASSERT_TRUE(field.is_string());

                        auto const& value = field.get_string();
                        EXPECT_EQ(value, NodeUtils::GetDesignation(m_server.GetNodeState()->GetOperation()));
                    }

                    {
                        auto const& field = object.at(symbols::NeighborCount);
                        ASSERT_TRUE(field.is_int64());

                        auto const& value = field.get_int64();
                        EXPECT_EQ(value, static_cast<std::int64_t>(m_server.GetPeerCache()->ActiveCount()));
                    }

                    {
                        auto const& field = object.at(symbols::Protocols);
                        ASSERT_TRUE(field.is_array());

                        auto const& value = field.get_array();
                        std::vector<std::string> protocols;
                        std::ranges::transform(value, std::back_inserter(protocols), [&](boost::json::value const& entry) -> std::string {
                            EXPECT_TRUE(entry.is_string());
                            return entry.as_string().c_str();
                        });
                        EXPECT_EQ(protocols, std::vector<std::string>{ std::string{ Network::TestScheme } });
                    }

                    {
                        auto const& field = object.at(symbols::UpdateTimestamp);
                        ASSERT_TRUE(field.is_int64());

                        auto const& value = field.get_int64();
                        EXPECT_EQ(value, m_server.GetNetworkState()->GetUpdatedTimepoint().time_since_epoch().count());
                    }

                    hasFoundServerResponse = true;
                } else {
                    // TODO: The entries for the other "peers" are manually inserted. In a real deployment
                    // (when notices are implemented), they contain the node info response. 
                    //EXPECT_TRUE(std::ranges::equal(entry.data, Route::Test::Message));
                }
            }
            EXPECT_TRUE(hasFoundServerResponse);
        },
        [&] (Peer::Action::Response const&) { ASSERT_FALSE(true); });
    EXPECT_TRUE(optTrackerKey);
    ASSERT_TRUE(m_optRequest);
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 1 });
    EXPECT_EQ(m_server.GetTrackingService()->Waiting(), std::size_t{ 0 });

    Peer::Action::Next next{ m_server.GetProxy(), *m_optRequest, m_server.GetServiceProvider() };
    EXPECT_TRUE(m_upFetchNodeHandler->OnMessage(*m_optRequest, next));
    EXPECT_FALSE(m_optResponse); // The response will not be sent from the server until all peers respond. 
    ASSERT_TRUE(next.GetTrackerKey()); // The next object should no have an associated tracker. 

    // Handling the fetch request should have caused an aggregate request tracker to be spawned.
    EXPECT_EQ(m_client.GetTrackingService()->Waiting(), std::size_t{ 1 });
    EXPECT_EQ(m_server.GetTrackingService()->Waiting(), std::size_t{ 1 });

    // TODO: The implementation of sending out the notice to the requisite peers is not yet implemented. 
    bool const result = m_server.GetPeerCache()->ForEach([&] (auto const& spIdentifier) {
        bool const result = m_server.GetTrackingService()->Process(
            *next.GetTrackerKey(), *spIdentifier, Message::Payload{ Route::Test::Message });
        return (result) ? CallbackIteration::Continue : CallbackIteration::Stop;
    }, IPeerCache::Filter::Active);
    EXPECT_TRUE(result);

    // After all responses are received, the aggregate tracker should be marked as completed.
    EXPECT_EQ(m_server.GetTrackingService()->Ready(), std::size_t{ 1 });
    EXPECT_EQ(m_server.GetTrackingService()->Execute(), std::size_t{ 1 });

    // After the aggregate tracker has been executed, the response to the client should have been sent. 
    ASSERT_TRUE(m_optResponse);
    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 0 });
    EXPECT_TRUE(m_client.GetTrackingService()->Process(std::move(*m_optResponse)));

    // The fetch node information response should cause the awaiting request to be fulfilled and executable 
    // on the next processing cycle. 
    EXPECT_EQ(m_client.GetTrackingService()->Ready(), std::size_t{ 1 });
    EXPECT_EQ(m_client.GetTrackingService()->Execute(), std::size_t{ 1 });
}

//----------------------------------------------------------------------------------------------------------------------

local::InformationResources::InformationResources(
    Node::SharedIdentifier const& spSelf, Node::SharedIdentifier const& spTarget, std::string_view binding)
    : m_spRegistrar(std::make_shared<Scheduler::Registrar>())
    , m_spServiceProvider(std::make_shared<Node::ServiceProvider>())
    , m_spTaskService(std::make_shared<Scheduler::TaskService>(m_spRegistrar))
    , m_spEventPublisher(std::make_shared<Event::Publisher>(m_spRegistrar))
    , m_spTrackingService(std::make_shared<Awaitable::TrackingService>(m_spRegistrar))
    , m_spCoordinatorState(std::make_shared<CoordinatorState>())
    , m_spNetworkState(std::make_shared<NetworkState>())
    , m_spNodeState(std::make_shared<NodeState>(spSelf, Network::ProtocolSet{}))
    , m_spBootstrapService(std::make_shared<BootstrapService>())
    , m_spNetworkManager()
    , m_spEndpoint()
    , m_spPeerCache(std::make_shared<Route::Test::PeerCache>(test::PeerCount))
    , m_context()
    , m_spProxy()
{
    m_spServiceProvider->Register(m_spTaskService);
    m_spServiceProvider->Register(m_spEventPublisher);
    m_spServiceProvider->Register(m_spCoordinatorState);
    m_spServiceProvider->Register(m_spNetworkState);
    m_spServiceProvider->Register(m_spNodeState);
    m_spServiceProvider->Register(m_spBootstrapService);
    m_spServiceProvider->Register<IPeerCache>(m_spPeerCache);
    m_spServiceProvider->Register(m_spTrackingService);
    
    auto options = Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(
        test::NetworkInterface, binding);
    m_endpoints.emplace_back(std::move(options));

    m_spNetworkManager = std::make_shared<Network::Manager>(test::RuntimeOptions.context, m_spServiceProvider);
    m_spServiceProvider->Register(m_spNetworkManager);

    m_spEndpoint = std::make_shared<Route::Test::StandardEndpoint>(Network::Endpoint::Properties{ options });
    m_spNetworkManager->RegisterEndpoint<InvokeContext::Test>(options, m_spEndpoint);

    m_spEventPublisher->SuspendSubscriptions();

    m_spProxy = Peer::Proxy::CreateInstance(*spTarget, m_spServiceProvider);
}

//----------------------------------------------------------------------------------------------------------------------
