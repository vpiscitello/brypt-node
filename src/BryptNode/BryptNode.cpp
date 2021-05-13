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
#include "Components/Configuration/Manager.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <random>
//----------------------------------------------------------------------------------------------------------------------

BryptNode::BryptNode(
    std::unique_ptr<Configuration::Manager> const& upConfiguration,
    std::shared_ptr<Event::Publisher> const& spEventPublisher,
    std::shared_ptr<Network::Manager> const& spNetworkManager,
    std::shared_ptr<Peer::Manager> const& spPeerManager,
    std::shared_ptr<AuthorizedProcessor> const& spMessageProcessor,
    std::shared_ptr<PeerPersistor> const& spPeerPersistor)
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
    , m_spPeerPersistor(spPeerPersistor)
    , m_handlers()
    , m_upRuntime(nullptr)
    , m_optShutdownCause()
{
    // An Network::Manager, Peer::Manager, and Configuration::Manager are required for the node to operate
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

    m_initialized = true;
}

//----------------------------------------------------------------------------------------------------------------------

bool BryptNode::Shutdown()
{
    if (!m_upRuntime) { return false; }

    m_spNetworkManager->Shutdown();

    bool const stopped = m_upRuntime->Stop();
    assert(stopped);

    m_upRuntime.reset();

    m_spEventPublisher->RegisterEvent<Event::Type::RuntimeStopped>({ Event::Cause::Expected });
    m_spEventPublisher->PublishEvents(); // Flush any pending events. 

    return stopped;
}

//----------------------------------------------------------------------------------------------------------------------

bool BryptNode::IsInitialized() const
{
    return m_initialized;
}

//----------------------------------------------------------------------------------------------------------------------

bool BryptNode::IsActive() const
{
    return (m_upRuntime && m_upRuntime->IsActive());
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<NodeState> BryptNode::GetNodeState() const
{
    return m_spNodeState;
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<CoordinatorState> BryptNode::GetCoordinatorState() const
{
    return m_spCoordinatorState;
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<NetworkState> BryptNode::GetNetworkState() const
{
    return m_spNetworkState;
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<SecurityState> BryptNode::GetSecurityState() const
{
    return m_spSecurityState;
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<SensorState> BryptNode::GetSensorState() const
{
    return m_spSensorState;
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Event::Publisher> BryptNode::GetEventPublisher() const
{
    return m_spEventPublisher;    
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Network::Manager> BryptNode::GetNetworkManager() const
{
    return m_spNetworkManager;
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Peer::Manager> BryptNode::GetPeerManager() const
{
    return m_spPeerManager;
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<Await::TrackingManager> BryptNode::GetAwaitManager() const
{
    return m_spAwaitManager;
}

//----------------------------------------------------------------------------------------------------------------------

std::weak_ptr<PeerPersistor> BryptNode::GetPeerPersistor() const
{
    return m_spPeerPersistor;
}

//----------------------------------------------------------------------------------------------------------------------

bool BryptNode::StartComponents()
{
    auto const OnError = [this] (ExecutionResult result) -> bool { m_optShutdownCause = result; return false; };
    if (!m_initialized) { return OnError(ExecutionResult::InitializationFailed); }
    if (m_upRuntime) { return OnError(ExecutionResult::AlreadyStarted);  }

    m_spLogger->info("Starting up brypt node instance.");
    m_spNetworkManager->Startup();
    m_spEventPublisher->RegisterEvent<Event::Type::RuntimeStarted>();

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
