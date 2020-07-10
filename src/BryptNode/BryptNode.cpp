//------------------------------------------------------------------------------------------------
// File: BryptNode.hpp
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
#include "../Components/Await/Await.hpp"
#include "../Components/Command/Handler.hpp"
#include "../Components/Endpoints/EndpointManager.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Components/MessageQueue/MessageQueue.hpp"
#include "../Components/PeerWatcher/PeerWatcher.hpp"
#include "../Configuration/PeerPersistor.hpp"
#include "../Utilities/Message.hpp"
#include "../Utilities/NodeUtils.hpp"
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

void SimulateClient(
    NodeUtils::NodeIdType const& id,
    Command::HandlerMap const& commandHandlers,
    bool activated);

//------------------------------------------------------------------------------------------------
} // test namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
CBryptNode::CBryptNode(
    NodeUtils::NodeIdType id,
    std::shared_ptr<CEndpointManager> const& spEndpointManager,
    std::shared_ptr<CMessageQueue> const& spMessageQueue,
    std::shared_ptr<CPeerPersistor> const& spPeerPersistor,
    Configuration::TSettings const& settings)
    : m_spNodeState()
    , m_spAuthorityState()
    , m_spCoordinatorState()
    , m_spNetworkState()
    , m_spSecurityState()
    , m_spSensorState()
    , m_spEndpointManager(spEndpointManager)
    , m_spMessageQueue(spMessageQueue)
    , m_spPeerPersistor(spPeerPersistor)
    , m_spAwaiting(std::make_shared<Await::CObjectContainer>())
    , m_commandHandlers()
    , m_spWatcher()
    , m_initialized(false)
{
    // An Endpoint Manager must be provided to the node in order to to operator 
    if (!m_spEndpointManager) {
        return;
    }

    // Initialize state from settings.
    m_spAuthorityState = std::make_shared<CAuthorityState>(settings.security.central_authority);
    m_spCoordinatorState = std::make_shared<CCoordinatorState>();
    m_spNetworkState = std::make_shared<CNetworkState>();
    m_spSecurityState = std::make_shared<CSecurityState>(settings.security.standard);
    m_spNodeState = std::make_shared<CNodeState>(id, m_spEndpointManager->GetEndpointTechnologies());
    m_spSensorState = std::make_shared<CSensorState>();

    // initialize peer watcher
    // m_spWatcher = std::make_shared<CPeerWatcher>(m_spEndpoints);

    m_commandHandlers.emplace(
        Command::Type::Information,
        Command::Factory(Command::Type::Information, *this));

    m_commandHandlers.emplace(
        Command::Type::Query,
        Command::Factory(Command::Type::Query, *this));

    m_commandHandlers.emplace(
        Command::Type::Election,
        Command::Factory(Command::Type::Election, *this));

    m_commandHandlers.emplace(
        Command::Type::Transform,
        Command::Factory(Command::Type::Transform, *this));

    m_commandHandlers.emplace(
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

    Listen(); // Make threaded operation?
}

//------------------------------------------------------------------------------------------------

bool CBryptNode::Shutdown()
{
    return m_spWatcher->Shutdown();
}

//------------------------------------------------------------------------------------------------

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

std::weak_ptr<CMessageQueue> CBryptNode::GetMessageQueue() const
{
    return m_spMessageQueue;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CPeerPersistor> CBryptNode::GetPeerPersistor() const
{
    return m_spPeerPersistor;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<Await::CObjectContainer> CBryptNode::GetAwaiting() const
{
    return m_spAwaiting;
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
    auto const technologies = m_spNodeState->GetTechnologies();
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
void CBryptNode::HandleQueueRequest(CMessage const& message)
{ 
    if (auto const itr = m_commandHandlers.find(message.GetCommandType()); itr != m_commandHandlers.end()) {
        itr->second->HandleMessage(message);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::ProcessFulfilledMessages()
{
    if (m_spAwaiting->IsEmpty()) {
        return;
    }

    std::vector<CMessage> const responses = m_spAwaiting->GetFulfilled();
    for (auto itr = responses.begin(); itr != responses.end(); ++itr) {
        m_spMessageQueue->PushOutgoingMessage(*itr);
    }
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
void CBryptNode::Listen()
{
    printo("Brypt Node is listening", NodeUtils::PrintType::Node);
    std::uint64_t run = 0;
    std::optional<CMessage> optQueueRequest;
    // TODO: Implement stopping condition
    do {
        optQueueRequest = m_spMessageQueue->PopIncomingMessage();
        if (optQueueRequest) {
            HandleQueueRequest(*optQueueRequest);
            optQueueRequest.reset();
        }

        ProcessFulfilledMessages();
        ++run;

        //----------------------------------------------------------------------------------------
        test::SimulateClient(m_spNodeState->GetId(), m_commandHandlers, (run % 10000 == 0));
        //----------------------------------------------------------------------------------------

        std::this_thread::sleep_for(local::CycleTimeout);
    } while (true);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void test::SimulateClient(
    NodeUtils::NodeIdType const& id,
    Command::HandlerMap const& commandHandlers,
    bool activated)
{
    if(!activated) {
        return;
    }

    std::cout << "== [Node] Simulating client sensor Information request" << '\n';
    CMessage informationRequest({}, 0xFFFFFFFF, id, Command::Type::Information, 0, "Request for Network Information.", 0);
    if (auto const itr = commandHandlers.find(informationRequest.GetCommandType()); itr != commandHandlers.end()) {
        itr->second->HandleMessage(informationRequest);
    }
    
    std::cout << "== [Node] Simulating client sensor Query request" << '\n';
    CMessage queryRequest({}, 0xFFFFFFFF, id, Command::Type::Query, 0, "Request for Sensor Readings.", 0);
    if (auto const itr = commandHandlers.find(queryRequest.GetCommandType()); itr != commandHandlers.end()) {
        itr->second->HandleMessage(queryRequest);
    }
}

//------------------------------------------------------------------------------------------------
