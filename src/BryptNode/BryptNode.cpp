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
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Manager.hpp"
#include "Components/Configuration/Parser.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

BryptNode::BryptNode(
    std::unique_ptr<Configuration::Parser> const& upConfiguration,
    std::shared_ptr<Event::Publisher> const& spEventPublisher,
    std::shared_ptr<Network::Manager> const& spNetworkManager,
    std::shared_ptr<Peer::Manager> const& spPeerManager,
    std::shared_ptr<AuthorizedProcessor> const& spMessageProcessor,
    std::shared_ptr<BootstrapService> const& spBootstrapService)
    : m_initialized(false)
    , m_spLogger(spdlog::get(LogUtils::Name::Core.data()))
    , m_spNodeState()
    , m_spCoordinatorState()
    , m_spNetworkState()
    , m_spSecurityState()
    , m_spSensorState()
    , m_spEventPublisher(spEventPublisher)
    , m_spNetworkManager(spNetworkManager)
    , m_spPeerManager(spPeerManager)
    , m_spMessageProcessor(spMessageProcessor)
    , m_spAwaitManager(std::make_shared<Await::TrackingManager>())
    , m_spBootstrapService(spBootstrapService)
    , m_handlers()
    , m_upRuntime(nullptr)
    , m_optShutdownCause()
{
    // An Network::Manager, Peer::Manager, and Configuration::Parser are required for the node to operate
    assert(m_spLogger && upConfiguration && m_spNetworkManager && m_spPeerManager);
    assert(upConfiguration->IsValidated());

    // Initialize state from settings.
    m_spCoordinatorState = std::make_shared<CoordinatorState>();
    m_spNetworkState = std::make_shared<NetworkState>();
    m_spSecurityState = std::make_shared<SecurityState>(
        upConfiguration->GetSecurityStrategy(), upConfiguration->GetCentralAuthority());
    m_spNodeState = std::make_shared<NodeState>(
        upConfiguration->GetNodeIdentifier(), m_spNetworkManager->GetEndpointProtocols());
    m_spSensorState = std::make_shared<SensorState>();

    m_handlers.emplace(Handler::Type::Information, Handler::Factory(Handler::Type::Information, *this));
    m_handlers.emplace(Handler::Type::Query, Handler::Factory(Handler::Type::Query, *this));
    m_handlers.emplace(Handler::Type::Election, Handler::Factory(Handler::Type::Election, *this));
    m_handlers.emplace(Handler::Type::Connect, Handler::Factory(Handler::Type::Connect, *this));

    spEventPublisher->Subscribe<Event::Type::CriticalNetworkFailure>(
        [this] () { m_optShutdownCause = ExecutionResult::UnexpectedShutdown; Shutdown(); });

    m_initialized = true;
}

//----------------------------------------------------------------------------------------------------------------------

void BryptNode::Shutdown()
{
    if (!m_upRuntime) { return; }
    m_spNetworkManager->Shutdown();
    m_upRuntime->Stop();
}

//----------------------------------------------------------------------------------------------------------------------

bool BryptNode::IsInitialized() const { return m_initialized; }

//----------------------------------------------------------------------------------------------------------------------

bool BryptNode::IsActive() const { return (m_upRuntime && m_upRuntime->IsActive()); }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<NodeState> BryptNode::GetNodeState() const { return m_spNodeState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<CoordinatorState> BryptNode::GetCoordinatorState() const { return m_spCoordinatorState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<NetworkState> BryptNode::GetNetworkState() const { return m_spNetworkState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<SecurityState> BryptNode::GetSecurityState() const { return m_spSecurityState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<SensorState> BryptNode::GetSensorState() const { return m_spSensorState; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Event::Publisher> BryptNode::GetEventPublisher() const { return m_spEventPublisher; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Network::Manager> BryptNode::GetNetworkManager() const { return m_spNetworkManager; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Peer::Manager> BryptNode::GetPeerManager() const { return m_spPeerManager; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<BootstrapService> BryptNode::GetBootstrapService() const { return m_spBootstrapService; }

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Await::TrackingManager> BryptNode::GetAwaitManager() const { return m_spAwaitManager; }

//----------------------------------------------------------------------------------------------------------------------

bool BryptNode::StartComponents()
{
    auto const OnError = [this] (ExecutionResult result) -> bool { m_optShutdownCause = result; return false; };
    if (!m_initialized) { return OnError(ExecutionResult::InitializationFailed); }
    if (m_upRuntime) { return OnError(ExecutionResult::AlreadyStarted);  }

    m_spLogger->info("Starting up brypt node instance.");
    m_spEventPublisher->SuspendSubscriptions(); // Event subscriptions are disabled after this point.
    m_spEventPublisher->Publish<Event::Type::RuntimeStarted>();
    m_spNetworkManager->Startup(); 

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void BryptNode::OnRuntimeStopped()
{
    m_upRuntime.reset(); // When the runtime notifies us it has stopped, it is now safe to destroy its resources. 
    using StopCause = Event::Message<Event::Type::RuntimeStopped>::Cause;
    m_spEventPublisher->Publish<Event::Type::RuntimeStopped>(StopCause::ShutdownRequest);
    m_spEventPublisher->Dispatch(); // Flush remaining events to the subscribers. 
}

//----------------------------------------------------------------------------------------------------------------------
