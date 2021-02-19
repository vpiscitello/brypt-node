//------------------------------------------------------------------------------------------------
// File: BryptNode.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
//------------------------------------------------------------------------------------------------
#include "CoordinatorState.hpp"
#include "NetworkState.hpp"
#include "NodeState.hpp"
#include "SecurityState.hpp"
#include "SensorState.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "Components/Await/TrackingManager.hpp"
#include "Components/BryptPeer/PeerManager.hpp"
#include "Components/Configuration/ConfigurationManager.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Handler/Handler.hpp"
#include "Components/MessageControl/AssociatedMessage.hpp"
#include "Components/MessageControl/AuthorizedProcessor.hpp"
#include "Components/Network/EndpointManager.hpp"
#include "Utilities/LogUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <random>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

constexpr std::chrono::nanoseconds CycleTimeout = std::chrono::milliseconds(1);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
BryptNode::BryptNode(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    std::shared_ptr<EndpointManager> const& spEndpointManager,
    std::shared_ptr<PeerManager> const& spPeerManager,
    std::shared_ptr<AuthorizedProcessor> const& spMessageProcessor,
    std::shared_ptr<PeerPersistor> const& spPeerPersistor,
    std::unique_ptr<Configuration::Manager> const& upConfigurationManager)
    : m_initialized(false)
    , m_spLogger(spdlog::get(LogUtils::Name::Core.data()))
    , m_spNodeState()
    , m_spCoordinatorState()
    , m_spNetworkState()
    , m_spSecurityState()
    , m_spSensorState()
    , m_spEndpointManager(spEndpointManager)
    , m_spPeerManager(spPeerManager)
    , m_spMessageProcessor(spMessageProcessor)
    , m_spAwaitManager(std::make_shared<Await::TrackingManager>())
    , m_spPeerPersistor(spPeerPersistor)
    , m_handlers()
{
    assert(m_spLogger);

    // An EndpointManager and PeerManager is required for the node to operate
    if (!m_spEndpointManager || !m_spPeerManager) {
        return;
    }

    // Initialize state from settings.
    m_spCoordinatorState = std::make_shared<CoordinatorState>();
    m_spNetworkState = std::make_shared<NetworkState>();
    m_spSecurityState = std::make_shared<SecurityState>(
        upConfigurationManager->GetSecurityStrategy(),
        upConfigurationManager->GetCentralAuthority());
    m_spNodeState = std::make_shared<NodeState>(
        spBryptIdentifier, m_spEndpointManager->GetEndpointProtocols());
    m_spSensorState = std::make_shared<SensorState>();

    m_handlers.emplace(
        Handler::Type::Information,
        Handler::Factory(Handler::Type::Information, *this));

    m_handlers.emplace(
        Handler::Type::Query,
        Handler::Factory(Handler::Type::Query, *this));

    m_handlers.emplace(
        Handler::Type::Election,
        Handler::Factory(Handler::Type::Election, *this));

    m_handlers.emplace(
        Handler::Type::Connect,
        Handler::Factory(Handler::Type::Connect, *this));

    m_initialized = true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void BryptNode::Startup()
{
    assert(m_initialized);
    m_spLogger->info("Starting up brypt node instance.");
    m_spEndpointManager->Startup();
    StartLifecycle();
}

//------------------------------------------------------------------------------------------------

bool BryptNode::Shutdown()
{
    return true;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<NodeState> BryptNode::GetNodeState() const
{
    return m_spNodeState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CoordinatorState> BryptNode::GetCoordinatorState() const
{
    return m_spCoordinatorState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<NetworkState> BryptNode::GetNetworkState() const
{
    return m_spNetworkState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<SecurityState> BryptNode::GetSecurityState() const
{
    return m_spSecurityState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<SensorState> BryptNode::GetSensorState() const
{
    return m_spSensorState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<EndpointManager> BryptNode::GetEndpointManager() const
{
    return m_spEndpointManager;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<PeerManager> BryptNode::GetPeerManager() const
{
    return m_spPeerManager;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<Await::TrackingManager> BryptNode::GetAwaitManager() const
{
    return m_spAwaitManager;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<PeerPersistor> BryptNode::GetPeerPersistor() const
{
    return m_spPeerPersistor;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void BryptNode::StartLifecycle()
{
    std::uint64_t run = 0;
    // TODO: Implement stopping condition
    do {
        if (auto const optMessage = m_spMessageProcessor->PopIncomingMessage(); optMessage) {
            HandleIncomingMessage(*optMessage);
        }

        m_spAwaitManager->ProcessFulfilledRequests();
        ++run;
        
        std::this_thread::sleep_for(local::CycleTimeout);
    } while (true);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void BryptNode::HandleIncomingMessage(AssociatedMessage const& associatedMessage)
{ 
    auto& [spBryptPeer, message] = associatedMessage;
    if (auto const itr = m_handlers.find(message.GetCommand()); itr != m_handlers.end()) {
        auto const& [type, handler] = *itr;
        handler->HandleMessage(associatedMessage);
    }
}

//------------------------------------------------------------------------------------------------
