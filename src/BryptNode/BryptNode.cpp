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
#include "../Components/Connection/Connection.hpp"
#include "../Components/Control/Control.hpp"
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

NodeUtils::TechnologyType InitialContactTechnology(NodeUtils::TechnologyType technology);

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
    , m_spConnections()
    , m_spControl()
    , m_spNotifier()
    , m_spWatcher()
    , m_initialized(false)
{
    // Initialize state from settings.
    m_spAuthorityState = std::make_shared<CAuthorityState>(settings.security.central_authority);
    m_spCoordinatorState = std::make_shared<CCoordinatorState>(
        settings.connections[0].technology,
        settings.connections[0].GetEntryComponents());
    m_spNetworkState = std::make_shared<CNetworkState>();
    m_spSecurityState = std::make_shared<CSecurityState>(settings.security.standard);
    m_spNodeState = std::make_shared<CNodeState>(
        settings.connections[0].interface,
        settings.connections[0].GetBindingComponents(),
        settings.details.operation,
        std::set<NodeUtils::TechnologyType>{settings.connections[0].technology});
    m_spSensorState = std::make_shared<CSensorState>();

    // initialize control
    m_spControl = std::make_shared<CControl>(
        m_spNodeState,
        m_spQueue.get(),
        m_spConnections);

    // initialize notifier
    NodeUtils::PortNumber const port = m_spNodeState->GetPublisherPort();
    m_spNotifier = std::make_shared<CNotifier>(port, m_spCoordinatorState, m_spConnections);

    // initialize peer watcher
    m_spWatcher = std::make_shared<CPeerWatcher>(m_spConnections);

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
            listen(); // Make threaded operation?
        } break;
        // Listen in thread?
        // Connect in another thread?
        // Bridge threads to receive upstream notifications and then pass down to own leafs
        // + pass aggregated messages to connect thread to respond with
        case NodeUtils::DeviceOperation::Branch: break;
        case NodeUtils::DeviceOperation::Leaf: {
            initialContact();  // Contact coordinator peer to get connection port
            connect(); // Make threaded operation?
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
// Description:
//------------------------------------------------------------------------------------------------
std::shared_ptr<CConnection> CBryptNode::SetupFullConnection(
    NodeUtils::NodeIdType id,
    std::string const& port,
    NodeUtils::TechnologyType technology)
{
    Configuration::TConnectionOptions options;
    options.id = id;
    options.technology = technology;
    options.operation = NodeUtils::ConnectionOperation::Server;
    options.binding = port;

    return Connection::Factory(m_spQueue.get(), options);;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------

void CBryptNode::NotifyConnection(NodeUtils::NodeIdType const& id)
{
    if (auto const itr = m_spConnections->find(id); itr != m_spConnections->end()) {
        itr->second->ResponseReady(id);
    }
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

std::weak_ptr<NodeUtils::ConnectionMap> CBryptNode::GetConnections() const
{
    return m_spConnections;
}

//------------------------------------------------------------------------------------------------

std::optional<std::weak_ptr<CConnection>> CBryptNode::GetConnection(NodeUtils::NodeIdType const& id) const
{
    if (auto const itr = m_spConnections->find(id); itr != m_spConnections->end()) {
        return itr->second;
    }
    return {};
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CControl> CBryptNode::GetControl() const
{
    return m_spControl;
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
std::float_t CBryptNode::determineNodePower()
{
    return 0.0;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType CBryptNode::determineConnectionType()
{
    return NodeUtils::TechnologyType::None;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType CBryptNode::determineBestConnectionType()
{
    // TODO: Actually determine the best technology type to use based on context
    auto const technologies = m_spNodeState->GetTechnologies();
    for (NodeUtils::TechnologyType const technologyType : technologies) {
        switch (technologyType) {
            case NodeUtils::TechnologyType::Direct:
            case NodeUtils::TechnologyType::StreamBridge:
            case NodeUtils::TechnologyType::TCP: return technologyType;
            case NodeUtils::TechnologyType::LoRa: return technologyType;
            case NodeUtils::TechnologyType::None: return technologyType;
        }
    }
    return NodeUtils::TechnologyType::None;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::hasTechnologyType(NodeUtils::TechnologyType technology)
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
bool CBryptNode::addConnection([[maybe_unused]] std::unique_ptr<CConnection> const& connection)
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::initialContact()
{
    NodeUtils::NodeIdType const id = m_spNodeState->GetId();
    Configuration::TConnectionOptions options;
    if (auto const technologies = m_spNodeState->GetTechnologies(); !technologies.empty()) {
        NodeUtils::TechnologyType const firstTechnologyType = (*technologies.begin());
        options.technology = local::InitialContactTechnology(firstTechnologyType);
    } else {
        // we don't support any technologies...
        return;
    }

    std::string const technologyTypeStr = std::to_string(static_cast<std::uint32_t>(options.technology));

    options.entry_address = m_spCoordinatorState->GetAddress() + NodeUtils::ADDRESS_COMPONENT_SEPERATOR + m_spCoordinatorState->GetRequestPort();

    printo("Connecting with initial contact technology: " +
            std::to_string(static_cast<std::uint32_t>(options.technology)) +
            " and on addr:port: " + options.entry_address,
            NodeUtils::PrintType::Node);

    std::shared_ptr<CConnection> const connection = Connection::Factory(m_spQueue.get(), options);

    std::optional<std::string> optResponse;
    printo("Sending coordinator acknowledgement", NodeUtils::PrintType::Node);
    connection->Send("\x06");   // Send initial ACK byte to peer
    optResponse = connection->Receive(0);  // Expect ACK back from peer
    if (!optResponse) {
        return;
    }
    printo("Received: " + std::to_string(static_cast<std::int32_t>(optResponse->c_str()[0])) + "\n", NodeUtils::PrintType::Node);

    // Send communication technology preferred
    printo("Sending preferred contact technology", NodeUtils::PrintType::Node);
    connection->Send(technologyTypeStr);
    optResponse = connection->Receive(0);  // Expect new connection port from peer or something related to LoRa/Bluetooth
    if (!optResponse) {
        return;
    }

    CMessage portMessage(*optResponse);
    optResponse.reset();
    Message::Buffer const buffer = portMessage.GetData();
    NodeUtils::PortNumber coordinatorPort(buffer.begin(), buffer.end());

    m_spCoordinatorState->SetId(portMessage.GetSourceId());
    m_spCoordinatorState->SetRequestPort(coordinatorPort); // Set the coordinator port to the dedicated port
    printo("Port received: " + coordinatorPort, NodeUtils::PrintType::Node);

    printo("Sending node information", NodeUtils::PrintType::Node);
    CMessage infoMessage(
        id, portMessage.GetSourceId(),
        Command::Type::Connect, 1,
        technologyTypeStr, 0);
    connection->Send(infoMessage);    // Send node information to peer

    optResponse = connection->Receive(0);  // Expect EOT back from peer
    printo("Received: " + std::to_string(static_cast<std::int32_t>(optResponse->c_str()[0])), NodeUtils::PrintType::Node);
    printo("Connection sequence completed. Connecting to new endpoint", NodeUtils::PrintType::Node);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    printo("Connection sequence completed. Shutting down initial connection", NodeUtils::PrintType::Node);
    connection->Shutdown(); // Shutdown initial connection
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::joinCoordinator()
{
    Configuration::TConnectionOptions options;

    options.operation = NodeUtils::ConnectionOperation::Client;

    NodeUtils::NodeIdType coordinatorId = m_spCoordinatorState->GetId();
    NodeUtils::NetworkAddress coordinatorAddress = m_spCoordinatorState->GetAddress();
    NodeUtils::PortNumber publisherPort = m_spCoordinatorState->GetPublisherPort();
    options.entry_address = coordinatorAddress + NodeUtils::ADDRESS_COMPONENT_SEPERATOR + m_spCoordinatorState->GetRequestPort();
    options.technology = m_spCoordinatorState->GetTechnology();

    printo("Connecting to Coordinator with technology: " +
            std::to_string(static_cast<std::uint32_t>(options.technology)),
            NodeUtils::PrintType::Node);

    m_spNotifier->Connect(coordinatorAddress, publisherPort);
    m_spConnections->emplace(coordinatorId, Connection::Factory(m_spQueue.get(), options));
    m_spNetworkState->PushPeerName(coordinatorId);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::contactAuthority()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::notifyAddressChange()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::handleControlRequest(std::string const& message)
{
    printo("Handling request from control socket", NodeUtils::PrintType::Node);

    if (message.empty()) {
        printo("No request to handle", NodeUtils::PrintType::Node);
        return;
    }

    try {
        CMessage const request(message);
        if (auto const itr = m_commandHandlers.find(request.GetCommandType()); itr != m_commandHandlers.end()) {
            itr->second->HandleMessage(request);
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
void CBryptNode::handleNotification(std::string const& message)
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
void CBryptNode::handleQueueRequest(CMessage const& message)
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
void CBryptNode::processFulfilledMessages()
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
bool CBryptNode::election()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CBryptNode::transform()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CBryptNode::listen()
{
    printo("Brypt Node is listening", NodeUtils::PrintType::Node);
    std::uint64_t run = 0;
    std::optional<std::string> optControlRequest;
    std::optional<std::string> optNotification;
    std::optional<CMessage> optQueueRequest;
    // TODO: Implement stopping condition
    do {
        optControlRequest = m_spControl->HandleRequest();
    	if (optControlRequest) {
    	    handleControlRequest(*optControlRequest);
    	    m_spControl->CloseCurrentConnection();
            optControlRequest.reset();
    	}

        optNotification = m_spNotifier->Receive();
    	if (optNotification) {
            handleNotification(*optNotification);
            optNotification.reset();
        }

        optQueueRequest = m_spQueue->PopIncomingMessage();
        if (optQueueRequest) {
            handleQueueRequest(*optQueueRequest);
            optQueueRequest.reset();
        }

        processFulfilledMessages();
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
void CBryptNode::connect(){
    printo("Brypt Node is connecting", NodeUtils::PrintType::Node);
    joinCoordinator();
    printo("Joined coordinator", NodeUtils::PrintType::Node);

    // TODO: Implement stopping condition
    std::uint64_t run = 0;
    std::optional<std::string> optNotification;
    do {
        optNotification = m_spNotifier->Receive();
    	if (optNotification) {
            handleNotification(*optNotification);
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
NodeUtils::TechnologyType local::InitialContactTechnology(NodeUtils::TechnologyType technology)
{
    switch (technology) {
        case NodeUtils::TechnologyType::LoRa:
        case NodeUtils::TechnologyType::None:
            return technology;
        case NodeUtils::TechnologyType::Direct:
        case NodeUtils::TechnologyType::TCP:
        case NodeUtils::TechnologyType::StreamBridge:
            return NodeUtils::TechnologyType::TCP;
    }
    return NodeUtils::TechnologyType::None;
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
