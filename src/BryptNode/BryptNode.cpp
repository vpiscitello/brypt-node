//------------------------------------------------------------------------------------------------
// File: BryptNode.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "BryptNode.hpp"
//------------------------------------------------------------------------------------------------
#include "AuthorityState.hpp"
#include "CoordinatorState.hpp"
#include "NetworkState.hpp"
#include "NodeState.hpp"
#include "SecurityState.hpp"
#include "SensorState.hpp"
#include "../BryptIdentifier/BryptIdentifier.hpp"
#include "../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../BryptMessage/ApplicationMessage.hpp"
#include "../Components/Await/TrackingManager.hpp"
#include "../Components/BryptPeer/PeerManager.hpp"
#include "../Components/Command/Handler.hpp"
#include "../Components/Endpoints/EndpointManager.hpp"
#include "../Components/MessageControl/AssociatedMessage.hpp"
#include "../Components/MessageControl/AuthorizedProcessor.hpp"
#include "../Configuration/ConfigurationManager.hpp"
#include "../Configuration/PeerPersistor.hpp"
#include "../Utilities/NodeUtils.hpp"
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
CBryptNode::CBryptNode(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    std::shared_ptr<CEndpointManager> const& spEndpointManager,
    std::shared_ptr<CPeerManager> const& spPeerManager,
    std::shared_ptr<CAuthorizedProcessor> const& spMessageProcessor,
    std::shared_ptr<CPeerPersistor> const& spPeerPersistor,
    std::unique_ptr<Configuration::CManager> const& upConfigurationManager)
    : m_initialized(false)
    , m_spNodeState()
    , m_spAuthorityState()
    , m_spCoordinatorState()
    , m_spNetworkState()
    , m_spSecurityState()
    , m_spSensorState()
    , m_spEndpointManager(spEndpointManager)
    , m_spPeerManager(spPeerManager)
    , m_spMessageProcessor(spMessageProcessor)
    , m_spAwaitManager(std::make_shared<Await::CTrackingManager>())
    , m_spPeerPersistor(spPeerPersistor)
    , m_handlers()
{
    // An EndpointManager and PeerManager is required for the node to operate
    if (!m_spEndpointManager || !m_spPeerManager) {
        return;
    }

    // Initialize state from settings.
    m_spAuthorityState = std::make_shared<CAuthorityState>(upConfigurationManager->GetCentralAuthority());
    m_spCoordinatorState = std::make_shared<CCoordinatorState>();
    m_spNetworkState = std::make_shared<CNetworkState>();
    m_spSecurityState = std::make_shared<CSecurityState>(upConfigurationManager->GetSecurityStandard());
    m_spNodeState = std::make_shared<CNodeState>(spBryptIdentifier, m_spEndpointManager->GetEndpointTechnologies());
    m_spSensorState = std::make_shared<CSensorState>();

    m_handlers.emplace(
        Command::Type::Information,
        Command::Factory(Command::Type::Information, *this));

    m_handlers.emplace(
        Command::Type::Query,
        Command::Factory(Command::Type::Query, *this));

    m_handlers.emplace(
        Command::Type::Election,
        Command::Factory(Command::Type::Election, *this));

    m_handlers.emplace(
        Command::Type::Connect,
        Command::Factory(Command::Type::Connect, *this));

    m_initialized = true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::Startup()
{
    if (m_initialized == false) {
        throw std::runtime_error("Invalid Brypt Node instance!");
    }

    printo("Starting up Brypt Node", NodeUtils::PrintType::Node);

    m_spEndpointManager->Startup();

    StartLifecycle();
}

//------------------------------------------------------------------------------------------------

bool CBryptNode::Shutdown()
{
    return true;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CNodeState> CBryptNode::GetNodeState() const
{
    return m_spNodeState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CAuthorityState> CBryptNode::GetAuthorityState() const
{
    return m_spAuthorityState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CCoordinatorState> CBryptNode::GetCoordinatorState() const
{
    return m_spCoordinatorState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CNetworkState> CBryptNode::GetNetworkState() const
{
    return m_spNetworkState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CSecurityState> CBryptNode::GetSecurityState() const
{
    return m_spSecurityState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CSensorState> CBryptNode::GetSensorState() const
{
    return m_spSensorState;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CEndpointManager> CBryptNode::GetEndpointManager() const
{
    return m_spEndpointManager;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CPeerManager> CBryptNode::GetPeerManager() const
{
    return m_spPeerManager;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<Await::CTrackingManager> CBryptNode::GetAwaitManager() const
{
    return m_spAwaitManager;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CPeerPersistor> CBryptNode::GetPeerPersistor() const
{
    return m_spPeerPersistor;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::StartLifecycle()
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
void CBryptNode::HandleIncomingMessage(AssociatedMessage const& associatedMessage)
{ 
    auto& [spBryptPeer, message] = associatedMessage;
    if (auto const itr = m_handlers.find(message.GetCommand()); itr != m_handlers.end()) {
        auto const& [type, handler] = *itr;
        handler->HandleMessage(associatedMessage);
    }
}

//------------------------------------------------------------------------------------------------
