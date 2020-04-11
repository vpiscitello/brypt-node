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
#include "../Components/Endpoints/Endpoint.hpp"
#include "../Components/MessageQueue/MessageQueue.hpp"
#include "../Components/Notifier/Notifier.hpp"
#include "../Components/PeerWatcher/PeerWatcher.hpp"
#include "../Utilities/Message.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
CBryptNode::CBryptNode(Configuration::TSettings const& settings)
    : m_spNodeState()
    , m_spAuthorityState()
    , m_spCoordinatorState()
    , m_spNetworkState()
    , m_spSecurityState()
    , m_spSensorState()
    , m_spQueue(std::make_shared<CMessageQueue>())
    , m_spAwaiting(std::make_shared<Await::CObjectContainer>())
    , m_commandHandlers()
    , m_spEndpoints()
    , m_spNotifier()
    , m_spWatcher()
    , m_initialized(false)
{
    // Initialize state from settings.
    m_spAuthorityState = std::make_shared<CAuthorityState>(settings.security.central_authority);
    m_spCoordinatorState = std::make_shared<CCoordinatorState>(
        settings.endpoints[0].technology,
        settings.endpoints[0].GetEntryComponents());
    m_spNetworkState = std::make_shared<CNetworkState>();
    m_spSecurityState = std::make_shared<CSecurityState>(settings.security.standard);
    m_spNodeState = std::make_shared<CNodeState>(
        settings.endpoints[0].interface,
        settings.endpoints[0].GetBindingComponents(),
        settings.details.operation,
        std::set<NodeUtils::TechnologyType>{settings.endpoints[0].technology});
    m_spSensorState = std::make_shared<CSensorState>();

    // initialize notifier
    NodeUtils::PortNumber const port = m_spNodeState->GetPublisherPort();
    m_spNotifier = std::make_shared<CNotifier>(port, m_spCoordinatorState, m_spEndpoints);

    // initialize peer watcher
    m_spWatcher = std::make_shared<CPeerWatcher>(m_spEndpoints);

    std::cout << "\n== Setting up Brypt Node\n";

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

CBryptNode::~CBryptNode()
{
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

    NodeUtils::DeviceOperation const operation = m_spNodeState->GetOperation();
    if (operation == NodeUtils::DeviceOperation::None) {
        throw std::runtime_error("Node Setup must be provided and device operation type!");
    }

    printo("Starting up Brypt Node", NodeUtils::PrintType::Node);

    srand(time(nullptr));

    switch (operation) {
        case NodeUtils::DeviceOperation::Root: {
            Listen(); // Make threaded operation?
        } break;
        // Listen in thread?
        // Connect in another thread?
        // Bridge threads to receive upstream notifications and then pass down to own leafs
        // + pass aggregated messages to connect thread to respond with
        case NodeUtils::DeviceOperation::Branch: break;
        case NodeUtils::DeviceOperation::Leaf: {
            Connect(); // Make threaded operation?
        } break;
        case NodeUtils::DeviceOperation::None: break;
    }
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

std::weak_ptr<CMessageQueue> CBryptNode::GetMessageQueue() const
{
    return m_spQueue;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<Await::CObjectContainer> CBryptNode::GetAwaiting() const
{
    return m_spAwaiting;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<NodeUtils::EndpointMap> CBryptNode::GetEndpoints() const
{
    return m_spEndpoints;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CNotifier> CBryptNode::GetNotifier() const
{
    return m_spNotifier;
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
bool CBryptNode::HasTechnologyType(NodeUtils::TechnologyType technology)
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
void CBryptNode::JoinCoordinator()
{
    Configuration::TEndpointOptions options;

    options.operation = NodeUtils::EndpointOperation::Client;

    NodeUtils::NodeIdType coordinatorId = m_spCoordinatorState->GetId();
    NodeUtils::NetworkAddress coordinatorAddress = m_spCoordinatorState->GetAddress();
    NodeUtils::PortNumber publisherPort = m_spCoordinatorState->GetPublisherPort();
    options.entry_address = coordinatorAddress + NodeUtils::ADDRESS_COMPONENT_SEPERATOR + m_spCoordinatorState->GetRequestPort();
    options.technology = m_spCoordinatorState->GetTechnology();

    printo("Connecting to Coordinator with technology: " +
            std::to_string(static_cast<std::uint32_t>(options.technology)),
            NodeUtils::PrintType::Node);

    m_spNotifier->Connect(coordinatorAddress, publisherPort);
    m_spEndpoints->emplace(coordinatorId, Endpoints::Factory(m_spQueue.get(), options));
    m_spNetworkState->PushPeerName(coordinatorId);
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
void CBryptNode::HandleNotification(std::string const& message)
{
    printo("Handling notification from coordinator", NodeUtils::PrintType::Node);

    if (message.empty()) {
        printo("No notification to handle", NodeUtils::PrintType::Node);
        return;
    }

    std::size_t const filterPosition = message.find(":");
    if (filterPosition == std::string::npos && filterPosition > 16) {
        return; // The notification is not properly formatted drop the message
    }

    // TODO: Do something with the filter (i.e. flood differently);
    std::string const filter = message.substr(0, filterPosition);
    std::string const raw = message.substr(filterPosition + 1);
    try {
        CMessage const notification(raw);
        if (auto const itr = m_commandHandlers.find(notification.GetCommandType()); itr != m_commandHandlers.end()) {
            itr->second->HandleMessage(notification);
        }
    } catch(...) {
        printo("Control message failed to unpack", NodeUtils::PrintType::Node);
        return;
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::HandleQueueRequest(CMessage const& message)
{
    printo("Handling queue request from connection thread", NodeUtils::PrintType::Node);
 
    if (auto const itr = m_commandHandlers.find(message.GetCommandType()); itr != m_commandHandlers.end()) {
        itr->second->HandleMessage(message);
    } else {
        printo("No command to handle", NodeUtils::PrintType::Node);
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::ProcessFulfilledMessages()
{
    printo("Sending off fulfilled requests", NodeUtils::PrintType::Node);

    if (m_spAwaiting->Empty()) {
        printo("No awaiting requests", NodeUtils::PrintType::Node);
        return;
    }

    printo("Fulfilled requests:", NodeUtils::PrintType::Node);
    std::vector<CMessage> const responses = m_spAwaiting->GetFulfilled();

    // /*
    for (auto itr = responses.begin(); itr != responses.end(); ++itr) {
        m_spQueue->PushOutgoingMessage(itr->GetDestinationId(), *itr);
    }
    // */
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
    std::optional<std::string> optControlRequest;
    std::optional<std::string> optNotification;
    std::optional<CMessage> optQueueRequest;
    // TODO: Implement stopping condition
    do {
        optNotification = m_spNotifier->Receive();
    	if (optNotification) {
            HandleNotification(*optNotification);
            optNotification.reset();
        }

        optQueueRequest = m_spQueue->PopIncomingMessage();
        if (optQueueRequest) {
            HandleQueueRequest(*optQueueRequest);
            optQueueRequest.reset();
        }

        ProcessFulfilledMessages();
        ++run;

        //----------------------------------------------------------------------------------------
        test::SimulateClient(m_spNodeState->GetId(), m_commandHandlers, (run % 10 == 0));
        //----------------------------------------------------------------------------------------

        std::this_thread::sleep_for(std::chrono::nanoseconds(1500));
    } while (true);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::Connect(){
    printo("Brypt Node is connecting", NodeUtils::PrintType::Node);
    JoinCoordinator();
    printo("Joined coordinator", NodeUtils::PrintType::Node);

    // TODO: Implement stopping condition
    std::uint64_t run = 0;
    std::optional<std::string> optNotification;
    do {
        optNotification = m_spNotifier->Receive();
    	if (optNotification) {
            HandleNotification(*optNotification);
            optNotification.reset();
        }

        ++run;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
    CMessage informationRequest(0xFFFFFFFF, id, Command::Type::Information, 0, "Request for Network Information.", 0);
    if (auto const itr = commandHandlers.find(informationRequest.GetCommandType()); itr != commandHandlers.end()) {
        itr->second->HandleMessage(informationRequest);
    }
    
    std::cout << "== [Node] Simulating client sensor Query request" << '\n';
    CMessage queryRequest(0xFFFFFFFF, id, Command::Type::Query, 0, "Request for Sensor Readings.", 0);
    if (auto const itr = commandHandlers.find(queryRequest.GetCommandType()); itr != commandHandlers.end()) {
        itr->second->HandleMessage(queryRequest);
    }
}

//------------------------------------------------------------------------------------------------
