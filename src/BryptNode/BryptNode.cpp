//----------------------------------------------------------------------------------------------------------------------
// File: BryptNode.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "CoordinatorState.hpp"
#include "NetworkState.hpp"
#include "NodeState.hpp"
#include "SecurityState.hpp"
#include "SensorState.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Await/TrackingManager.hpp"
#include "Components/Event/Publisher.hpp"
#include "Components/Handler/Handler.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/MessageControl/DiscoveryProtocol.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Manager.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Node::Core::Core(
    std::reference_wrapper<ExecutionToken> const& token,
    std::unique_ptr<Configuration::Parser> const& upParser,
    std::shared_ptr<BootstrapService> const& spBootstrapService)
    : m_token(token)
    , m_upRuntime(nullptr)
    , m_logger(spdlog::get(LogUtils::Name::Core.data()))
    , m_spNodeState()
    , m_spCoordinatorState(std::make_shared<CoordinatorState>())
    , m_spNetworkState(std::make_shared<NetworkState>())
    , m_spSecurityState(std::make_shared<SecurityState>(
        upParser->GetSecurityStrategy(), upParser->GetCentralAuthority()))
    , m_spSensorState(std::make_shared<SensorState>())
    , m_spEventPublisher(std::make_shared<Event::Publisher>())
    , m_spNetworkManager()
    , m_spPeerManager()
    , m_spMessageProcessor(std::make_shared<AuthorizedProcessor>(upParser->GetNodeIdentifier()))
    , m_spAwaitManager(std::make_shared<Await::TrackingManager>())
    , m_spBootstrapService(spBootstrapService)
    , m_handlers()
{
    // The Configuration::Parser and Logger must be valid to initialize the node core.
    assert(m_logger && upParser);
    assert(upParser->IsValidated());

    {
        // Create the message handlers for the supported application message types. Note: Network message
        // handling is determined by the enabled processor for the peer and will not be forwarded into the core. 
        m_handlers.emplace(Handler::Type::Information, Handler::Factory(Handler::Type::Information, *this));
        m_handlers.emplace(Handler::Type::Query, Handler::Factory(Handler::Type::Query, *this));
        m_handlers.emplace(Handler::Type::Election, Handler::Factory(Handler::Type::Election, *this));
        m_handlers.emplace(Handler::Type::Connect, Handler::Factory(Handler::Type::Connect, *this));
    }

    {
        // Make a discovery protocol such that the peers can automatically perform a connection procedure without
        // forwarding messages into the core. 
        auto const spProtocol = std::make_shared<DiscoveryProtocol>(upParser->GetEndpointOptions());
        m_spPeerManager = std::make_shared<Peer::Manager>(
            upParser->GetNodeIdentifier(), upParser->GetSecurityStrategy(),
            m_spEventPublisher, spProtocol, m_spMessageProcessor);
    }

    {
        // If we should perform the inital connection bootstrapping based on the stored peers, then provide the 
        // network manager a bootstrap cache to use. Otherwise, do not provide the cache and no inital connections
        // will be scheduled. 
        IBootstrapCache const* const pBootstraps = (upParser->UseBootstraps() ? spBootstrapService.get() : nullptr);
        m_spNetworkManager = std::make_shared<Network::Manager>(
            upParser->GetRuntimeContext(), upParser->GetEndpointOptions(),
            m_spEventPublisher, m_spPeerManager.get(), pBootstraps);

        // On a critical network error, use the token to stop the core runtime loop and signal an unexpected error.
        m_spEventPublisher->Subscribe<Event::Type::CriticalNetworkFailure>([this] { OnUnexpectedError(); });
    }

    m_spNodeState = std::make_shared<NodeState>(
        upParser->GetNodeIdentifier(), m_spNetworkManager->GetEndpointProtocols());

    spBootstrapService->SetMediator(m_spPeerManager.get());
}

//----------------------------------------------------------------------------------------------------------------------

Node::Core::~Core()
{
    // Note: The ResourceShutdown status is a variant of RequestedShutdown that indicates the runtime shouldn't 
    // attempt to use resources that may have been destroyed (e.g. user provided event listeners).
    Shutdown(ExecutionStatus::ResourceShutdown);
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
    }

    return m_token.get().Status();
}

//----------------------------------------------------------------------------------------------------------------------

bool Node::Core::IsActive() const
{
    return m_token.get().IsExecutionActive();
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

std::weak_ptr<SensorState> Node::Core::GetSensorState() const { return m_spSensorState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Event::Publisher> Node::Core::GetEventPublisher() const { return m_spEventPublisher; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Network::Manager> Node::Core::GetNetworkManager() const { return m_spNetworkManager; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Peer::Manager> Node::Core::GetPeerManager() const { return m_spPeerManager; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<BootstrapService> Node::Core::GetBootstrapService() const { return m_spBootstrapService; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Await::TrackingManager> Node::Core::GetAwaitManager() const { return m_spAwaitManager; }

//----------------------------------------------------------------------------------------------------------------------

ExecutionStatus Node::Core::StartComponents()
{
    if (!m_token.get().RequestStart({})) { return ExecutionStatus::AlreadyStarted; }

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
