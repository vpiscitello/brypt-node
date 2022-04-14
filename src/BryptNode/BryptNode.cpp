//----------------------------------------------------------------------------------------------------------------------
// File: BryptNode.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Route/MessageHandler.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Manager.hpp"
#include "Components/Route/Connect.hpp"
#include "Components/Route/Information.hpp"
#include "Components/Route/Router.hpp"
#include "Components/Scheduler/Registrar.hpp"
#include "Components/Scheduler/TaskService.hpp"
#include "Components/State/CoordinatorState.hpp"
#include "Components/State/NetworkState.hpp"
#include "Components/State/NodeState.hpp"
#include "Components/State/SecurityState.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Node::Core::Core(std::reference_wrapper<ExecutionToken> const& token)
    : m_token(token)
    , m_logger(spdlog::get(Logger::Name::Core.data()))
    , m_spServiceProvider(std::make_shared<ServiceProvider>())
    , m_spScheduler(std::make_shared<Scheduler::Registrar>())
    , m_upRuntime(nullptr)
    , m_spNodeState()
    , m_spCoordinatorState(std::make_shared<CoordinatorState>())
    , m_spNetworkState(std::make_shared<NetworkState>())
    , m_spSecurityState()
    , m_spTaskService(std::make_shared<Scheduler::TaskService>(m_spScheduler))
    , m_spEventPublisher(std::make_shared<Event::Publisher>(m_spScheduler))
    , m_spRouter(std::make_shared<Route::Router>())
    , m_spTrackingService(std::make_shared<Awaitable::TrackingService>(m_spScheduler))
    , m_spDiscoveryProtocol()
    , m_spNetworkManager()
    , m_spPeerManager()
    , m_spMessageProcessor()
    , m_spBootstrapService()
    , m_initialized(false)
{
    assert(m_logger);
    CreateStaticResources();
}

//----------------------------------------------------------------------------------------------------------------------

Node::Core::Core(
    std::reference_wrapper<ExecutionToken> const& token,
    std::unique_ptr<Configuration::Parser> const& upParser,
    std::shared_ptr<BootstrapService> const& spBootstrapService)
    : m_token(token)
    , m_logger(spdlog::get(Logger::Name::Core.data()))
    , m_spServiceProvider(std::make_shared<ServiceProvider>())
    , m_spScheduler(std::make_shared<Scheduler::Registrar>())
    , m_upRuntime(nullptr)
    , m_spNodeState()
    , m_spCoordinatorState(std::make_shared<CoordinatorState>())
    , m_spNetworkState(std::make_shared<NetworkState>())
    , m_spSecurityState()
    , m_spTaskService(std::make_shared<Scheduler::TaskService>(m_spScheduler))
    , m_spEventPublisher(std::make_shared<Event::Publisher>(m_spScheduler))
    , m_spRouter(std::make_shared<Route::Router>())
    , m_spTrackingService(std::make_shared<Awaitable::TrackingService>(m_spScheduler))
    , m_spDiscoveryProtocol()
    , m_spNetworkManager()
    , m_spPeerManager()
    , m_spMessageProcessor()
    , m_spBootstrapService()
    , m_initialized(false)
{
    assert(m_logger);
    CreateStaticResources();
    [[maybe_unused]] bool const result = CreateConfiguredResources(upParser, spBootstrapService);
}

//----------------------------------------------------------------------------------------------------------------------

Node::Core::~Core()
{
    // Note: The ResourceShutdown status is a variant of RequestedShutdown that indicates the runtime shouldn't 
    // attempt to use resources that may have been destroyed (e.g. user provided event listeners).
    Shutdown(ExecutionStatus::ResourceShutdown);
    if (m_spBootstrapService) { m_spBootstrapService->UnregisterServices(); }
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::Core::IsInitialized() const noexcept { return m_initialized; }

//----------------------------------------------------------------------------------------------------------------------

bool Node::Core::IsActive() const noexcept { return m_token.get().IsExecutionActive(); }

//----------------------------------------------------------------------------------------------------------------------

bool Node::Core::CreateConfiguredResources(
    std::unique_ptr<Configuration::Parser> const& upParser,
    std::shared_ptr<BootstrapService> const& spBootstrapService)
{
    if (m_token.get().Status() != ExecutionStatus::Standby) { return false; }

    // The Configuration::Parser and Logger must be valid to initialize the node core.
    assert(upParser && upParser->Validated());
    assert(m_spRouter->Contains(Route::Fundamental::Connect::DiscoveryHandler::Path));
    assert(m_spRouter->Contains(Route::Fundamental::Information::NodeHandler::Path));
    assert(m_spRouter->Contains(Route::Fundamental::Information::FetchNodeHandler::Path));

    auto const& spIdentifier = upParser->GetNodeIdentifier();
    auto const strategy = upParser->GetSecurityStrategy();
    auto const& endpoints = upParser->GetEndpoints();

    // Save the applicable configured state to be used during execution. 
    {
        Network::ProtocolSet protocols;
        std::ranges::transform(endpoints, std::inserter(protocols, protocols.begin()), [] (auto const& endpoint) {
            return endpoint.GetProtocol();
        });

        m_spNodeState = std::make_shared<NodeState>(spIdentifier, protocols);
        m_spServiceProvider->Register(m_spNodeState);
        
        m_spSecurityState = std::make_shared<SecurityState>(strategy);
        m_spServiceProvider->Register(m_spSecurityState);
    }

    // Create the main execution services, these components will drive the main execution loop by notifying 
    // the scheduler when work becomes available. 
    {
        m_spMessageProcessor = std::make_shared<AuthorizedProcessor>(m_spScheduler, m_spServiceProvider);
        m_spServiceProvider->Register<IMessageSink>(m_spMessageProcessor);
    }

    // If we should perform the inital connection bootstrapping based on the stored peers, then provide the 
    // network manager a bootstrap cache to use. Otherwise, do not provide the cache and no inital connections
    // will be scheduled. 
    {
        auto const context = upParser->GetRuntimeContext();
        m_spNetworkManager = std::make_shared<Network::Manager>(context, m_spServiceProvider);
        m_spServiceProvider->Register(m_spNetworkManager);
    }

    // Make a discovery protocol such that the peers can automatically perform a connection procedure without
    // forwarding messages into the core. 
    {
        m_spDiscoveryProtocol = std::make_shared<Route::Fundamental::Connect::DiscoveryProtocol>(
            endpoints, m_spServiceProvider);
        m_spServiceProvider->Register<IConnectProtocol>(m_spDiscoveryProtocol);

        m_spPeerManager = std::make_shared<Peer::Manager>(strategy, m_spServiceProvider);
        m_spServiceProvider->Register<IPeerMediator>(m_spPeerManager);
        m_spServiceProvider->Register<IPeerCache>(m_spPeerManager);
    }

    // Store the provided bootstrap service and configure it with the node's resouces. 
    {
        m_spBootstrapService = spBootstrapService;
        m_spBootstrapService->Register(m_spPeerManager.get());
        m_spBootstrapService->Register(m_spScheduler);
        m_spServiceProvider->Register(m_spBootstrapService);
    }

    if (!m_spNetworkManager->Attach(endpoints, m_spServiceProvider)) { return false; }
    
    m_initialized = true;
    
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::Core::Attach(Configuration::Options::Endpoint const& options)
{
    return m_spNetworkManager ? m_spNetworkManager->Attach(options, m_spServiceProvider) : true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::Core::Detach(Configuration::Options::Endpoint const& options)
{
    return m_spNetworkManager ? m_spNetworkManager->Detach(options) : true;
}

//----------------------------------------------------------------------------------------------------------------------

ExecutionStatus Node::Core::Shutdown(ExecutionStatus reason)
{
    #if !defined(NDEBUG)
    constexpr auto IsTokenInExpectedState = [] (bool success, ExecutionToken const& token) -> bool
    {
        // If we are the first to request the stop, then the token's status should indicate a requested shutdown. 
        // Otherwise, a non-executing state is expected (e.g. we have already stopped due to a prior shutdown request). 
        if (auto const status = token.Status(); success) { return status == ExecutionStatus::RequestedShutdown; }
        else { return status != ExecutionStatus::Executing; }
    };
    #endif 

    bool const success = m_token.get().RequestStop(reason);
    assert(IsTokenInExpectedState(success, m_token)); // Ensure the token is in a non-executing state.
    
    if (success) {
        // Note: Given this call was able to request the stop and the runtime is operating in a thread, the runtime
        // is expected to have fully completed execution and the resources can be destroyed. In the foreground context,
        // this is handled by the call to the start method. 
        if (m_upRuntime && m_upRuntime->Type() == RuntimeContext::Background) { m_upRuntime.reset(); }
        assert(Assertions::Threading::RegisterCoreThread()); // Reset the core thread after the runtime has joined.
    }

    return m_token.get().Status();
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<NodeState> Node::Core::GetNodeState() const { return m_spNodeState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<CoordinatorState> Node::Core::GetCoordinatorState() const { return m_spCoordinatorState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<NetworkState> Node::Core::GetNetworkState() const { return m_spNetworkState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<SecurityState> Node::Core::GetSecurityState() const { return m_spSecurityState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Event::Publisher> Node::Core::GetEventPublisher() const { return m_spEventPublisher; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Awaitable::TrackingService> Node::Core::GetTrackingService() const { return m_spTrackingService; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Network::Manager> Node::Core::GetNetworkManager() const { return m_spNetworkManager; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Peer::Manager> Node::Core::GetPeerManager() const { return m_spPeerManager; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<BootstrapService> Node::Core::GetBootstrapService() const { return m_spBootstrapService; }

//----------------------------------------------------------------------------------------------------------------------

void Node::Core::CreateStaticResources()
{
    m_spServiceProvider->Register(m_spCoordinatorState);
    m_spServiceProvider->Register(m_spNetworkState);
    m_spServiceProvider->Register(m_spTaskService);
    m_spServiceProvider->Register(m_spEventPublisher);
    m_spServiceProvider->Register(m_spRouter);
    m_spServiceProvider->Register(m_spTrackingService);

    // Register the default application message routes. Platform messages are not routable and are handled 
    // internally by the message processors. 
    {
        [[maybe_unused]] bool const success = m_spRouter->Register<Route::Fundamental::Connect::DiscoveryHandler>(
            Route::Fundamental::Connect::DiscoveryHandler::Path);
        assert(success); // This route should always successfully register. 
    }
    {
        [[maybe_unused]] bool const success = m_spRouter->Register<Route::Fundamental::Information::NodeHandler>(
            Route::Fundamental::Information::NodeHandler::Path);
        assert(success); // This route should always successfully register. 
    }
    {
        [[maybe_unused]] bool const success = m_spRouter->Register<Route::Fundamental::Information::FetchNodeHandler>(
            Route::Fundamental::Information::FetchNodeHandler::Path);
        assert(success); // This route should always successfully register. 
    }

    // On a critical network error, use the token to stop the core runtime loop and signal an unexpected error.
    m_spEventPublisher->Subscribe<Event::Type::CriticalNetworkFailure>([this] { OnUnexpectedError(); });
}

//----------------------------------------------------------------------------------------------------------------------

ExecutionStatus Node::Core::StartComponents()
{
    if (!m_token.get().RequestStart({})) { return ExecutionStatus::AlreadyStarted; }

    assert(m_initialized);

    // Initialize the scheduler to set the priority of execution. If it fails, one of the executable services must
    // have a cyclic dependency. 
    if (!m_spScheduler->Initialize()) { return ExecutionStatus::InitializationFailed; }

    // Initialize the router to ensure the message handlers can use the service provider to get their requisite
    // dependencies. The objects registered may change between runs, so this must occur before each cycle starts. 
    if (!m_spRouter->Initialize(m_spServiceProvider)) { return ExecutionStatus::InitializationFailed; }

    assert(m_spEventPublisher->EventCount() == std::size_t(0)); // All events should be flushed between cycles. 
    m_spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.
    m_spEventPublisher->Publish<Event::Type::RuntimeStarted>(); // Publish the first event indicating execution start. 
    m_spNetworkManager->Startup();

    return ExecutionStatus::Standby;
}

//----------------------------------------------------------------------------------------------------------------------

void Node::Core::OnRuntimeStopped(ExecutionStatus status)
{
    m_spNetworkManager->Shutdown();

    // Note: Durning the destruction of the core it is no longer safe to use the event publisher. Some subscribers
    // may have been destroyed and may not be executed. 
    if (status != ExecutionStatus::ResourceShutdown) {
        using StopCause = Event::Message<Event::Type::RuntimeStopped>::Cause;
        m_spEventPublisher->Publish<Event::Type::RuntimeStopped>(StopCause::ShutdownRequest);
        m_spEventPublisher->Dispatch(); // Flush remaining events to the subscribers. 
        assert(!m_token.get().IsExecutionActive() && m_token.get().Status() == ExecutionStatus::Standby);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Node::Core::OnUnexpectedError()
{
    #if !defined(NDEBUG)
    constexpr auto IsTokenInExpectedState = [] (bool success, ExecutionToken const& token) -> bool
    {
        // If we are the first to request the stop, then the token's status should indicate an unexpected shutdown. 
        // Otherwise, a non-executing state is expected (e.g. we have already stopped due to a prior error). 
        if (auto const status = token.Status(); success) {  return status == ExecutionStatus::UnexpectedShutdown; }
        else { return status != ExecutionStatus::Executing; }
    };
    #endif 

    [[maybe_unused]] bool const success = m_token.get().RequestStop(ExecutionStatus::UnexpectedShutdown);
    assert(IsTokenInExpectedState(success, m_token)); // Ensure the token is in a non-executing state.
}

//----------------------------------------------------------------------------------------------------------------------
