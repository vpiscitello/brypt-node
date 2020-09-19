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
#include "../Components/Await/TrackingManager.hpp"
#include "../Components/BryptPeer/PeerManager.hpp"
#include "../Components/Command/Handler.hpp"
#include "../Components/Endpoints/EndpointManager.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Components/MessageControl/AssociatedMessage.hpp"
#include "../Components/MessageControl/MessageCollector.hpp"
#include "../Components/PeerWatcher/PeerWatcher.hpp"
#include "../Configuration/ConfigurationManager.hpp"
#include "../Configuration/PeerPersistor.hpp"
#include "../Message/MessageBuilder.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <random>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

constexpr std::chrono::nanoseconds CycleTimeout = std::chrono::milliseconds(1);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace test {
//------------------------------------------------------------------------------------------------

void SimulateClient(Command::HandlerMap const& handlers, bool activated);

//------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
CBryptNode::CBryptNode(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    std::shared_ptr<CEndpointManager> const& spEndpointManager,
    std::shared_ptr<CPeerManager> const& spPeerManager,
    std::shared_ptr<CMessageCollector> const& spMessageCollector,
    std::shared_ptr<CPeerPersistor> const& spPeerPersistor,
    std::unique_ptr<Configuration::CManager> const& upConfigurationManager)
    : m_spNodeState()
    , m_spAuthorityState()
    , m_spCoordinatorState()
    , m_spNetworkState()
    , m_spSecurityState()
    , m_spSensorState()
    , m_spEndpointManager(spEndpointManager)
    , m_spPeerManager(spPeerManager)
    , m_spMessageCollector(spMessageCollector)
    , m_spAwaitManager(std::make_shared<Await::CTrackingManager>())
    , m_spWatcher()
    , m_handlers()
    , m_spPeerPersistor(spPeerPersistor)
    , m_initialized(false)
{
    // An Endpoint Manager must be provided to the node in order to to operator 
    if (!m_spEndpointManager) {
        return;
    }

    // Initialize state from settings.
    m_spAuthorityState = std::make_shared<CAuthorityState>(upConfigurationManager->GetCentralAuthority());
    m_spCoordinatorState = std::make_shared<CCoordinatorState>();
    m_spNetworkState = std::make_shared<CNetworkState>();
    m_spSecurityState = std::make_shared<CSecurityState>(upConfigurationManager->GetSecurityStandard());
    m_spNodeState = std::make_shared<CNodeState>(spBryptIdentifier, m_spEndpointManager->GetEndpointTechnologies());
    m_spSensorState = std::make_shared<CSensorState>();

    // initialize peer watcher
    // m_spWatcher = std::make_shared<CPeerWatcher>(m_spEndpoints);

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
        Command::Type::Transform,
        Command::Factory(Command::Type::Transform, *this));

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
        throw std::runtime_error("Node instance must be setup before starting!");
    }

    printo("Starting up Brypt Node", NodeUtils::PrintType::Node);

    m_spEndpointManager->Startup();
    std::this_thread::sleep_for(local::CycleTimeout);

    StartLifecycle(); // Make threaded operation?
}

//------------------------------------------------------------------------------------------------

bool CBryptNode::Shutdown()
{
    return m_spWatcher->Shutdown();
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

std::weak_ptr<CMessageCollector> CBryptNode::GetMessageCollector() const
{
    return m_spMessageCollector;
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
    printo("Brypt Node is listening", NodeUtils::PrintType::Node);
    std::uint64_t run = 0;
    // TODO: Implement stopping condition
    do {
        if (auto const optMessage = m_spMessageCollector->PopIncomingMessage(); optMessage) {
            HandleIncomingMessage(*optMessage);
        }

        m_spAwaitManager->ProcessFulfilledRequests();
        ++run;

        //----------------------------------------------------------------------------------------
        test::SimulateClient(m_handlers, (run % 10000 == 0));
        //----------------------------------------------------------------------------------------

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
    if (auto const itr = m_handlers.find(message.GetCommandType()); itr != m_handlers.end()) {
        auto const& [type, handler] = *itr;
        handler->HandleMessage(associatedMessage);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::float_t CBryptNode::DetermineNodePower()
{
    return 0.0;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::HasTechnologyType(Endpoints::TechnologyType technology)
{
    auto const technologies = m_spEndpointManager->GetEndpointTechnologies();
    auto const itr = technologies.find(technology);
    return (itr != technologies.end());
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::ContactAuthority()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::NotifyAddressChange()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::Election()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::Transform()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void test::SimulateClient(Command::HandlerMap const& handlers, bool activated)
{
    if(!activated) {
        return;
    }

    thread_local std::random_device device;
    thread_local std::mt19937 generator(device());
    thread_local std::bernoulli_distribution distribution(0.5);

    Command::Type command = Command::Type::Invalid;
    std::string data = {};

    bool bUseInformationRequest = distribution(generator);
    if (bUseInformationRequest) {
        printo("Simulating node information request", NodeUtils::PrintType::Node); 
        command = Command::Type::Information;
        data =  "Request for node information.";
    } else {
        printo("Simulating sensor query request", NodeUtils::PrintType::Node);
        command = Command::Type::Query;
        data =  "Request for sensor readings.";
    }

    OptionalMessage const optRequest = CMessage::Builder()
        .SetSource(ReservedIdentifiers::Network::Unknown)
        .SetDestination(ReservedIdentifiers::Network::ClusterRequest)
        .SetCommand(command, 0)
        .SetData(data, 0)
        .ValidatedBuild();
    assert(optRequest);

    if (auto const itr = handlers.find(optRequest->GetCommandType()); itr != handlers.end()) {
        auto const& [command, handler] = *itr;
        handler->HandleMessage(AssociatedMessage{ {}, *optRequest });
    }
}

//------------------------------------------------------------------------------------------------
