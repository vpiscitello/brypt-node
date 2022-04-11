//----------------------------------------------------------------------------------------------------------------------
#include "TestHelpers.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/RuntimeContext.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Network/TCP/Endpoint.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/BootstrapCache.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

class EventObserver;
class BootstrapCache;
class TargetResources;

using ConfigurationResources = std::pair<Configuration::Options::Endpoints, std::shared_ptr<IBootstrapCache>>;
std::optional<ConfigurationResources> CreateConfigurationResources(std::string_view uri);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

constexpr RuntimeContext Context = RuntimeContext::Foreground;

auto const OriginIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
auto const TargetIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());

constexpr std::string_view Interface = "lo";
constexpr std::string_view OriginBinding = "*:35216";
constexpr std::size_t ExpectedEndpoints = 2;

auto TargetOptions = Configuration::Options::Endpoint{ Network::Protocol::TCP, Interface, "*:35217" };

//----------------------------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

class local::EventObserver
{
public:
    using BindingFailure = Event::Message<Event::Type::BindingFailed>::Cause;

    explicit EventObserver(Event::SharedPublisher const& spEventPublisher);
    bool SubscribedToAllAdvertisedEvents() const;
    bool ReceivedExpectedEvents(Event::Type type, std::size_t count) { return m_events[type] == count; }
    bool ReceivedAnyErrorEvents() const { return m_hasUnexpectedError; }
    bool ReceivedExpectedFailure(BindingFailure failure) const { return m_optFailure && *m_optFailure == failure; }

private:
    using EventCounter = std::unordered_map<Event::Type, std::size_t>;

    Event::SharedPublisher m_spEventPublisher;
    EventCounter m_events;
    std::optional<BindingFailure> m_optFailure;
    bool m_hasUnexpectedError; // A flag for any errors we don't explicitly test for. 
};

//----------------------------------------------------------------------------------------------------------------------

class local::BootstrapCache : public IBootstrapCache
{
public:
    BootstrapCache() : m_bootstraps() {}

    void InsertBootstrap(Network::RemoteAddress const& bootstrap)
    {
        m_bootstraps.emplace(bootstrap);
    }

    // IBootstrapCache {
    bool Contains(Network::RemoteAddress const& address) const override
    {
        return m_bootstraps.contains(address);
    }

    std::size_t ForEachBootstrap(BootstrapReader const&) const override { return 0; }
    std::size_t ForEachBootstrap(Network::Protocol protocol, BootstrapReader const& reader) const override
    {
        if (m_bootstraps.empty()) { return 0; }   

        std::size_t read = 0;
        for (auto const& bootstrap : m_bootstraps) {
            if (bootstrap.GetProtocol() == protocol) { continue; }
            if (++read; reader(bootstrap) != CallbackIteration::Continue) { break; }
        }
        return read;
    }

    std::size_t BootstrapCount() const override { return 0; }
    std::size_t BootstrapCount(Network::Protocol) const override { return 0; }
    // } IBootstrapCache

private:
    BootstrapService::BootstrapCache m_bootstraps;
};

//----------------------------------------------------------------------------------------------------------------------

class local::TargetResources
{
public:
    TargetResources() = default;

    [[nodiscard]] bool Initialize(Configuration::Options::Endpoint const& options);
    void Destroy();

private:
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    std::shared_ptr<Event::Publisher> m_spEventPublisher;
    std::shared_ptr<IMessageSink> m_spMessageProcessor;
    std::shared_ptr<IPeerMediator> m_spPeerMediator;
    std::unique_ptr<Network::IEndpoint> m_upEndpoint;
};

//----------------------------------------------------------------------------------------------------------------------

class NetworkManagerSuite : public testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        // Make a target endpoint that the managed endpoints can connect to. Currently, we do not test any information
        // on the target resources directly.
        ASSERT_TRUE(test::TargetOptions.Initialize(Network::Test::RuntimeOptions, spdlog::get(Logger::Name::Core.data())));
        ASSERT_TRUE(m_target.Initialize(test::TargetOptions));
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give the target a chance to spin up. 
    }

    static void TearDownTestSuite()
    {
        m_target.Destroy(); // Destroy our static resources before implicit destruction occurs. 
    }

    void SetUp() override
    {
        // Create the resources required for each instance of a test. 
        m_spRegistrar = std::make_shared<Scheduler::Registrar>();
        m_spServiceProvider = std::make_shared<Node::ServiceProvider>();
        
        m_spTaskService = std::make_shared<Scheduler::TaskService>(m_spRegistrar);
        m_spServiceProvider->Register(m_spTaskService);
        
        m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
        m_spServiceProvider->Register(m_spEventPublisher);

        m_upEventObserver = std::make_unique<local::EventObserver>(m_spEventPublisher);

        m_spMessageProcessor = std::make_shared<Network::Test::MessageProcessor>(test::OriginIdentifier);
        m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);

        m_spPeerMediator = std::make_shared<Network::Test::SinglePeerMediator>(
            test::OriginIdentifier, m_spMessageProcessor.get(), m_spServiceProvider);
        m_spServiceProvider->Register<IPeerMediator>(m_spPeerMediator);

        ASSERT_TRUE(m_spRegistrar->Initialize());
    }

    static local::TargetResources m_target;
    std::shared_ptr<Scheduler::Registrar> m_spRegistrar;
    std::shared_ptr<Node::ServiceProvider> m_spServiceProvider;
    std::shared_ptr<Scheduler::TaskService> m_spTaskService;
    Event::SharedPublisher m_spEventPublisher;
    std::unique_ptr<local::EventObserver> m_upEventObserver;
    std::shared_ptr<NodeState> m_spNodeState;
    std::shared_ptr<IMessageSink> m_spMessageProcessor;
    std::shared_ptr<IPeerMediator> m_spPeerMediator;
};

//----------------------------------------------------------------------------------------------------------------------

local::TargetResources NetworkManagerSuite::m_target = {};

//----------------------------------------------------------------------------------------------------------------------

TEST_F(NetworkManagerSuite, LifecycleTest)
{
    // Make the configuration resources for our Network::Manager. 
    auto optConfiguration = local::CreateConfigurationResources(test::OriginBinding);
    ASSERT_TRUE(optConfiguration);
    auto const& [configurations, spBootstrapCache] = *optConfiguration;
    ASSERT_EQ(configurations.size() * 2, test::ExpectedEndpoints);
    m_spServiceProvider->Register<IBootstrapCache>(spBootstrapCache);
    
    // Create our Network::Manager to start the tests of its operations and state. 
    auto const upNetworkManager = std::make_unique<Network::Manager>(
        Network::Test::RuntimeOptions.context, m_spServiceProvider);
    ASSERT_TRUE(upNetworkManager->Attach(configurations, m_spServiceProvider));
    EXPECT_EQ(m_spRegistrar->Execute(), std::size_t{ 1 });
    
    EXPECT_TRUE(m_upEventObserver->SubscribedToAllAdvertisedEvents());
    m_spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.
    EXPECT_EQ(upNetworkManager->ActiveEndpointCount(), std::size_t{ 0 });
    EXPECT_EQ(upNetworkManager->ActiveProtocolCount(), std::size_t{ 0 });

    upNetworkManager->Startup();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give the endpoints a chance to spin up. 
    EXPECT_GT(m_spEventPublisher->Dispatch(), std::size_t{ 0 });

    // Test the effects and state of starting up the manager. 
    EXPECT_EQ(upNetworkManager->ActiveEndpointCount(), test::ExpectedEndpoints);
    EXPECT_EQ(upNetworkManager->ActiveProtocolCount(), configurations.size());
    EXPECT_TRUE(upNetworkManager->IsRegisteredAddress(configurations.front().GetBinding()));
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedEvents(Event::Type::EndpointStarted, test::ExpectedEndpoints));

    auto const spServerEndpoint = upNetworkManager->GetEndpoint(Network::Protocol::TCP, Network::Operation::Server);
    EXPECT_TRUE(spServerEndpoint);
    EXPECT_EQ(spServerEndpoint, upNetworkManager->GetEndpoint(spServerEndpoint->GetIdentifier()));
    EXPECT_EQ(upNetworkManager->GetEndpointBinding(
            spServerEndpoint->GetIdentifier()), configurations.front().GetBinding());

    auto const spClientEndpoint = upNetworkManager->GetEndpoint(Network::Protocol::TCP, Network::Operation::Client);
    EXPECT_TRUE(spClientEndpoint);
    EXPECT_EQ(spClientEndpoint, upNetworkManager->GetEndpoint(spClientEndpoint->GetIdentifier()));
    EXPECT_EQ(upNetworkManager->GetEndpointBinding(spClientEndpoint->GetIdentifier()), Network::BindingAddress{});
    
    upNetworkManager->Shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give the endpoints a chance to shutdown. 
    EXPECT_GT(m_spEventPublisher->Dispatch(), std::size_t{ 0 });

    // Test the effects and state of shutting down the manager. 
    EXPECT_EQ(upNetworkManager->ActiveEndpointCount(), std::size_t{ 0 });
    EXPECT_EQ(upNetworkManager->ActiveProtocolCount(), std::size_t{ 0 });
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedEvents(Event::Type::EndpointStopped, test::ExpectedEndpoints));
    EXPECT_FALSE(m_upEventObserver->ReceivedAnyErrorEvents());
}

//----------------------------------------------------------------------------------------------------------------------

TEST_F(NetworkManagerSuite, CriticalShutdownTest)
{
    // Make a configuration that will cause an unrecoverable error in a spawned endpoint. We need to use a binding 
    // that will fail for reason other than being malformed, becuase those errore would be caught by initalizing the 
    // address in the settings. Here are using the target's binding because it is known to be in use. 
    auto optConfiguration = local::CreateConfigurationResources(test::TargetOptions.GetBinding().GetUri());
    ASSERT_TRUE(optConfiguration);
    auto const& [configurations, spBootstrapCache] = *optConfiguration;
    ASSERT_EQ(configurations.size() * 2, test::ExpectedEndpoints);
    m_spServiceProvider->Register<IBootstrapCache>(spBootstrapCache);

    // Create our Network::Manager to start the tests of its operations and state. 
    // Note: Most of the stored state in the manager should be the same as the lifecycle tests, the differences will 
    // be primarily observed through the events published. 
    auto const upNetworkManager = std::make_unique<Network::Manager>(
        Network::Test::RuntimeOptions.context, m_spServiceProvider);
    ASSERT_TRUE(upNetworkManager->Attach(configurations, m_spServiceProvider));
    EXPECT_EQ(m_spRegistrar->Execute(), 1);

    EXPECT_TRUE(m_upEventObserver->SubscribedToAllAdvertisedEvents());
    m_spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.
    EXPECT_EQ(upNetworkManager->ActiveEndpointCount(), std::size_t{ 0 });
    EXPECT_EQ(upNetworkManager->ActiveProtocolCount(), std::size_t{ 0 });

    upNetworkManager->Startup();
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give the endpoints a chance to spin up. 

    // The network shutdown event will be published when the binding failure handler is invoked in this call. 
    // The events fired by an event handler are currently handled on the next dispatch cycle. 
    EXPECT_GT(m_spEventPublisher->Dispatch(), std::size_t{ 0 });
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedEvents(Event::Type::EndpointStarted, test::ExpectedEndpoints));
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedEvents(Event::Type::BindingFailed, configurations.size()));
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Give the endpoints a chance to shutdown. 

    EXPECT_EQ(upNetworkManager->ActiveEndpointCount(), std::size_t{ 0 });
    EXPECT_EQ(upNetworkManager->ActiveProtocolCount(), std::size_t{ 0 });

    // We need to call dispatch a second time to fire the events published during the first invocation. 
    EXPECT_GT(m_spEventPublisher->Dispatch(), std::size_t{ 0 }); 
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedEvents(Event::Type::BindingFailed, 1));
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedFailure(local::EventObserver::BindingFailure::AddressInUse));
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedEvents(Event::Type::CriticalNetworkFailure, 1));

    // The cache state before a network shutdown should not be cleared when a critical error occurs. 
    EXPECT_TRUE(upNetworkManager->IsRegisteredAddress(configurations.front().GetBinding())); 
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedEvents(Event::Type::EndpointStopped, test::ExpectedEndpoints));
    
    m_spEventPublisher->Dispatch(); // Verify that only one critical error has been published. 
    EXPECT_TRUE(m_upEventObserver->ReceivedExpectedEvents(Event::Type::CriticalNetworkFailure, 1));
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<local::ConfigurationResources> local::CreateConfigurationResources(std::string_view uri)
{
    Configuration::Options::Endpoints configured;
    {
        Configuration::Options::Endpoint options{ Network::Protocol::TCP, test::Interface, uri };
        if (!options.Initialize(Network::Test::RuntimeOptions, spdlog::get(Logger::Name::Core.data()))) {
            return {};
        }
        configured.emplace_back(options);
    }

    auto upBootstraps = std::make_shared<local::BootstrapCache>();
    {
        using Origin = Network::RemoteAddress::Origin;
        auto const& binding = configured.front().GetBinding();
        Network::RemoteAddress address(binding.GetProtocol(), binding.GetUri(), true, Origin::User);
        upBootstraps->InsertBootstrap(address);
    }

    return std::make_pair(std::move(configured), std::move(upBootstraps));
}

//----------------------------------------------------------------------------------------------------------------------

bool local::TargetResources::Initialize(Configuration::Options::Endpoint const& options)
{
    m_spRegistrar = std::make_shared<Scheduler::Registrar>();
    m_spServiceProvider = std::make_shared<Node::ServiceProvider>();

    m_spTaskService = std::make_shared<Scheduler::TaskService>(m_spRegistrar);
    m_spServiceProvider->Register(m_spTaskService);

    m_spEventPublisher = std::make_shared<Event::Publisher>(m_spRegistrar);
    m_spServiceProvider->Register(m_spEventPublisher);

    m_spMessageProcessor = std::make_unique<Network::Test::MessageProcessor>(test::TargetIdentifier);
    m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);

    m_spPeerMediator = std::make_unique<Network::Test::SinglePeerMediator>(
        test::TargetIdentifier, m_spMessageProcessor.get(), m_spServiceProvider);
    m_spServiceProvider->Register<IPeerMediator>(m_spPeerMediator);
    
    auto const properties = Network::Endpoint::Properties{ Network::Operation::Server, options };
    m_upEndpoint = std::make_unique<Network::TCP::Endpoint>(properties);
    m_upEndpoint->Register(m_spEventPublisher);
    m_upEndpoint->Register(m_spPeerMediator.get());

    if (!m_upEndpoint->ScheduleBind(options.GetBinding())) { return false; }
    m_upEndpoint->Startup();

    m_spEventPublisher->SuspendSubscriptions(); // We don't need to subscribe to any of the target's events. 

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void local::TargetResources::Destroy()
{
    m_upEndpoint.reset();
    m_spPeerMediator.reset();
    m_spMessageProcessor.reset();
    m_spEventPublisher.reset();
    m_spTaskService.reset();
    m_spServiceProvider.reset();
    m_spRegistrar.reset();
}

//----------------------------------------------------------------------------------------------------------------------

local::EventObserver::EventObserver(Event::SharedPublisher const& spEventPublisher)
    : m_spEventPublisher(spEventPublisher)
    , m_events()
    , m_hasUnexpectedError(false)
{
    // Subscribe to all events fired by an endpoint. Each listener should only record valid events. 
    m_spEventPublisher->Subscribe<Event::Type::EndpointStarted>([&] (auto, auto, auto) {
        ++m_events[Event::Type::EndpointStarted];
    });

    m_spEventPublisher->Subscribe<Event::Type::EndpointStopped>([&] (auto, auto, auto, auto) {
        ++m_events[Event::Type::EndpointStopped];
    });

    m_spEventPublisher->Subscribe<Event::Type::BindingFailed>([&] (auto, auto const&, auto failure) {
        m_hasUnexpectedError = true;
        ++m_events[Event::Type::BindingFailed];
        m_optFailure = failure;
    });

    m_spEventPublisher->Subscribe<Event::Type::ConnectionFailed>([&] (auto, auto const&, auto) {
        m_hasUnexpectedError = true;
        ++m_events[Event::Type::ConnectionFailed];
    });

    m_spEventPublisher->Subscribe<Event::Type::CriticalNetworkFailure>([&] () {
        m_hasUnexpectedError = true;
        ++m_events[Event::Type::CriticalNetworkFailure];
    });
}

//----------------------------------------------------------------------------------------------------------------------

bool local::EventObserver::SubscribedToAllAdvertisedEvents() const
{
    // We expect to be subscribed to all events advertised by an endpoint. A failure here is most likely caused
    // by this test fixture being outdated. 
    return m_spEventPublisher->ListenerCount() == m_spEventPublisher->AdvertisedCount();
}

//----------------------------------------------------------------------------------------------------------------------
