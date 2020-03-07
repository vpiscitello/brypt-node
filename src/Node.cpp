//------------------------------------------------------------------------------------------------
// File: Node.hpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Node.hpp"
//------------------------------------------------------------------------------------------------
#include "State.hpp"
#include "Components/Await/Await.hpp"
#include "Components/Command/Handler.hpp"
#include "Components/Connection/Connection.hpp"
#include "Components/Control/Control.hpp"
#include "Components/MessageQueue/MessageQueue.hpp"
#include "Components/Notifier/Notifier.hpp"
#include "Components/PeerWatcher/PeerWatcher.hpp"
#include "Utilities/Message.hpp"
#include "Utilities/NodeUtils.hpp"
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
    std::shared_ptr<CState> const& state,
    NodeUtils::CommandMap const& commands,
    bool activated);

//------------------------------------------------------------------------------------------------
} // test namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
CNode::CNode(Configuration::TSettings const& settings)
    : m_state(std::make_shared<CState>(settings))
    , m_queue(std::make_shared<CMessageQueue>())
    , m_awaiting(std::make_shared<Await::CObjectContainer>())
    , m_commands()
    , m_connections()
    , m_control(std::make_shared<CControl>(
        m_state,
        m_queue.get(),
        m_connections))
    , m_notifier(std::make_shared<CNotifier>(m_state, m_connections))
    , m_watcher(std::make_shared<CPeerWatcher>(m_state, m_connections))
    , m_initialized(false)
{
    if (settings.connections[0].operation == NodeUtils::ConnectionOperation::None) {
        std::cout << "Node Setup must be provided and device operation type!" << std::endl;
        exit(-1);
    }

    std::cout << "\n== Setting up Brypt Node\n";

    m_commands.emplace(
        NodeUtils::CommandType::Information,
        Command::Factory(NodeUtils::CommandType::Information, *this, m_state));

    m_commands.emplace(
        NodeUtils::CommandType::Query,
        Command::Factory(NodeUtils::CommandType::Query, *this, m_state));

    m_commands.emplace(
        NodeUtils::CommandType::Election,
        Command::Factory(NodeUtils::CommandType::Election, *this, m_state));

    m_commands.emplace(
        NodeUtils::CommandType::Transform,
        Command::Factory(NodeUtils::CommandType::Transform, *this, m_state));

    m_commands.emplace(
        NodeUtils::CommandType::Connect,
        Command::Factory(NodeUtils::CommandType::Connect, *this, m_state));

    m_initialized = true;
}

//------------------------------------------------------------------------------------------------

CNode::~CNode()
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void CNode::Startup()
{
    if (m_initialized == false) {
        throw std::runtime_error("Node instance must be setup before starting!");
    }

    NodeUtils::DeviceOperation operation = NodeUtils::DeviceOperation::None;
    if (auto const selfState = m_state->GetSelfState().lock()) {
        operation = selfState->GetOperation();
    }

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
        case NodeUtils::DeviceOperation::Lead: {
            initialContact();  // Contact coordinator peer to get connection port
            connect(); // Make threaded operation?
        } break;
        case NodeUtils::DeviceOperation::None: break;
    }
}

//------------------------------------------------------------------------------------------------

bool CNode::Shutdown()
{
    return m_watcher->Shutdown();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::shared_ptr<CConnection> CNode::SetupFullConnection(
    NodeUtils::NodeIdType id,
    std::string const& port,
    NodeUtils::TechnologyType technology)
{
    Configuration::TConnectionOptions options;
    options.id = id;
    options.technology = technology;
    options.operation = NodeUtils::ConnectionOperation::Server;
    options.binding = port;

    return Connection::Factory(m_queue.get(), options);;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------

void CNode::NotifyConnection(NodeUtils::NodeIdType const& id)
{
    if (auto const itr = m_connections->find(id); itr != m_connections->end()) {
        itr->second->ResponseReady(id);
    }
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CMessageQueue> CNode::GetMessageQueue() const
{
    return m_queue;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<Await::CObjectContainer> CNode::GetAwaiting() const
{
    return m_awaiting;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<NodeUtils::ConnectionMap> CNode::GetConnections() const
{
    return m_connections;
}

//------------------------------------------------------------------------------------------------

std::optional<std::weak_ptr<CConnection>> CNode::GetConnection(NodeUtils::NodeIdType const& id) const
{
    if (auto const itr = m_connections->find(id); itr != m_connections->end()) {
        return itr->second;
    }
    return {};
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CControl> CNode::GetControl() const
{
    return m_control;
}

//------------------------------------------------------------------------------------------------

std::weak_ptr<CNotifier> CNode::GetNotifier() const
{
    return m_notifier;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::float_t CNode::determineNodePower()
{
    return 0.0;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType CNode::determineConnectionType()
{
    return NodeUtils::TechnologyType::None;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType CNode::determineBestConnectionType()
{
    auto const selfState = m_state->GetSelfState().lock();
    if (selfState) {
        return NodeUtils::TechnologyType::None;
    }
    // TODO: Actually determine the best technology type to use based on context
    auto const technologies = selfState->GetTechnologies();
    for (auto itr = technologies.begin(); itr != technologies.end(); ++itr) {
        switch (*itr) {
            case NodeUtils::TechnologyType::Direct:
            case NodeUtils::TechnologyType::StreamBridge:
            case NodeUtils::TechnologyType::TCP: return *itr;
            case NodeUtils::TechnologyType::LoRa: return *itr;
            case NodeUtils::TechnologyType::None: return *itr;
        }
    }
    return NodeUtils::TechnologyType::None;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CNode::hasTechnologyType(NodeUtils::TechnologyType technology)
{
    auto const selfState = m_state->GetSelfState().lock();
    if (selfState) {
        return false;
    }

    auto const technologies = selfState->GetTechnologies();
    if (auto itr = technologies.find(technology); itr != technologies.end()) {
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CNode::addConnection([[maybe_unused]] std::unique_ptr<CConnection> const& connection)
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CNode::initialContact()
{
    NodeUtils::NodeIdType id = 0;
    Configuration::TConnectionOptions options;
    if (auto const selfState = m_state->GetSelfState().lock()) {
        id = selfState->GetId();
        options.technology = local::InitialContactTechnology(*selfState->GetTechnologies().begin());;
    } else { return; }

    char const* const technologyTypeStr = std::to_string(static_cast<std::uint32_t>(options.technology)).c_str();

    if (auto const coordinatorState = m_state->GetCoordinatorState().lock()) {
        options.entry_address = coordinatorState->GetAddress() + coordinatorState->GetRequestPort();
    } else { return; }

    printo("Connecting with initial contact technology: " +
            std::to_string(static_cast<std::uint32_t>(options.technology)) +
            " and on addr:port: " + options.entry_address,
            NodeUtils::PrintType::Node);

    std::shared_ptr<CConnection> const connection = Connection::Factory(m_queue.get(), options);

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
    if (auto const coordinatorState = m_state->GetCoordinatorState().lock()) {
        coordinatorState->SetId(portMessage.GetSourceId());
        coordinatorState->SetRequestPort(coordinatorPort); // Set the coordinator port to the dedicated port
        printo("Port received: " + coordinatorPort, NodeUtils::PrintType::Node);
    } else { return; }

    printo("Sending node information", NodeUtils::PrintType::Node);
    CMessage infoMessage(
        id, portMessage.GetSourceId(),
        NodeUtils::CommandType::Connect, 1,
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
void CNode::joinCoordinator()
{
    Configuration::TConnectionOptions options;

    options.operation = NodeUtils::ConnectionOperation::Client;

    NodeUtils::NodeIdType coordinatorId = 0;
    NodeUtils::NetworkAddress coordinatorAddress = std::string();
    NodeUtils::PortNumber publisherPort  = std::string();
    if (auto const coordinatorState = m_state->GetCoordinatorState().lock()) {
        coordinatorId = coordinatorState->GetId();
        coordinatorAddress = coordinatorState->GetAddress();
        options.entry_address = coordinatorAddress + coordinatorState->GetRequestPort();
        options.technology = coordinatorState->GetTechnology();
        publisherPort = coordinatorState->GetPublisherPort();
    } else { return; }

    printo("Connecting to Coordinator with technology: " +
            std::to_string(static_cast<std::uint32_t>(options.technology)),
            NodeUtils::PrintType::Node);

    m_notifier->Connect(coordinatorAddress, publisherPort);
    m_connections->emplace(coordinatorId, Connection::Factory(m_queue.get(), options));
    if (auto const networkState = m_state->GetNetworkState().lock()) {
        networkState->PushPeerName(coordinatorId);
    } else { return; }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CNode::contactAuthority()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CNode::notifyAddressChange()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CNode::handleControlRequest(std::string const& message)
{
    printo("Handling request from control socket", NodeUtils::PrintType::Node);

    if (message.empty()) {
        printo("No request to handle", NodeUtils::PrintType::Node);
        return;
    }

    try {
        CMessage const request(message);
        if (auto const itr = m_commands.find(request.GetCommand()); itr != m_commands.end()) {
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
void CNode::handleNotification(std::string const& message)
{
    printo("Handling notification from coordinator", NodeUtils::PrintType::Node);

    if (message.empty()) {
        printo("No notification to handle", NodeUtils::PrintType::Node);
        return;
    }

    std::size_t const filterPosition = message.find(":");
    if(filterPosition == std::string::npos && filterPosition > 16) {
        return; // The notification is not properly formatted drop the message
    }

    // TODO: Do something with the filter (i.e. flood differently);
    std::string const filter = message.substr(0, filterPosition);
    std::string const raw = message.substr(filterPosition + 1);
    try {
        CMessage const notification(raw);
        if (auto const itr = m_commands.find(notification.GetCommand()); itr != m_commands.end()) {
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
void CNode::handleQueueRequest(CMessage const& message)
{
    printo("Handling queue request from connection thread", NodeUtils::PrintType::Node);
    NodeUtils::CommandType const command = message.GetCommand();

    if (command == NodeUtils::CommandType::None) {
        printo("No command to handle", NodeUtils::PrintType::Node);
        return;
    }

    m_commands[command]->HandleMessage(message);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CNode::processFulfilledMessages()
{
    printo("Sending off fulfilled requests", NodeUtils::PrintType::Node);

    if (m_awaiting->Empty()) {
        printo("No awaiting requests", NodeUtils::PrintType::Node);
        return;
    }

    printo("Fulfilled requests:", NodeUtils::PrintType::Node);
    std::vector<CMessage> const responses = m_awaiting->GetFulfilled();

    // /*
    for (auto itr = responses.begin(); itr != responses.end(); ++itr) {
        m_queue->PushOutgoingMessage(itr->GetDestinationId(), *itr);
    }
    // */
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CNode::election()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CNode::transform()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CNode::listen()
{
    printo("Brypt Node is listening", NodeUtils::PrintType::Node);
    std::uint64_t run = 0;
    std::optional<std::string> optControlRequest;
    std::optional<std::string> optNotification;
    std::optional<CMessage> optQueueRequest;
    // TODO: Implement stopping condition
    do {
        optControlRequest = m_control->HandleRequest();
    	if (optControlRequest) {
    	    handleControlRequest(*optControlRequest);
    	    m_control->CloseCurrentConnection();
            optControlRequest.reset();
    	}

        optNotification = m_notifier->Receive();
    	if (optNotification) {
            handleNotification(*optNotification);
            optNotification.reset();
        }

        optQueueRequest = m_queue->PopIncomingMessage();
        if (optQueueRequest) {
            handleQueueRequest(*optQueueRequest);
            optQueueRequest.reset();
        }

        processFulfilledMessages();
        ++run;

        //----------------------------------------------------------------------------------------
        test::SimulateClient(m_state, m_commands, (run % 10 == 0));
        //----------------------------------------------------------------------------------------

        std::this_thread::sleep_for(std::chrono::nanoseconds(1500));
    } while (true);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CNode::connect(){
    printo("Brypt Node is connecting", NodeUtils::PrintType::Node);
    joinCoordinator();
    printo("Joined coordinator", NodeUtils::PrintType::Node);

    // TODO: Implement stopping condition
    std::uint64_t run = 0;
    std::optional<std::string> optNotification;
    do {
        optNotification = m_notifier->Receive();
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
    std::shared_ptr<CState> const& state,
    NodeUtils::CommandMap const& commands,
    bool activated)
{
    if(!activated) {
        return;
    }

    NodeUtils::NodeIdType id = 0;
    if (auto const selfState = state->GetSelfState().lock()) {
        id = selfState->GetId();
    } else { return; }

    std::cout << "== [Node] Simulating client sensor Information request" << '\n';
    CMessage informationRequest(0xFFFFFFFF, id, NodeUtils::CommandType::Information, 0, "Request for Network Information.", 0);
    if (auto const itr = commands.find(informationRequest.GetCommand()); itr != commands.end()) {
        itr->second->HandleMessage(informationRequest);
    }
    
    std::cout << "== [Node] Simulating client sensor Query request" << '\n';
    CMessage queryRequest(0xFFFFFFFF, id, NodeUtils::CommandType::Query, 0, "Request for Sensor Readings.", 0);
    if (auto const itr = commands.find(queryRequest.GetCommand()); itr != commands.end()) {
        itr->second->HandleMessage(queryRequest);
    }
}

//------------------------------------------------------------------------------------------------
