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
NodeUtils::TechnologyType InitialContactTechnology(NodeUtils::TechnologyType const& technology);
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
CNode::CNode(NodeUtils::TOptions const& options)
    : m_state(std::make_shared<CState>(options))
    , m_queue(std::make_shared<CMessageQueue>())
    , m_awaiting(std::make_shared<Await::CObjectContainer>())
    , m_commands()
    , m_connections()
    , m_control(std::make_shared<CControl>(m_state, m_connections, NodeUtils::TechnologyType::TCP))
    , m_notifier(std::make_shared<CNotifier>(m_state, m_connections))
    , m_watcher(std::make_shared<CPeerWatcher>(m_state, m_connections))
    , m_initialized(false)
{
    if (options.m_operation == NodeUtils::DeviceOperation::NONE) {
        std::cout << "Node Setup must be provided and device operation type!" << std::endl;
        exit(-1);
    }

    std::cout << "\n== Setting up Brypt Node\n";

    m_commands.emplace(
        NodeUtils::CommandType::INFORMATION,
        Command::Factory(NodeUtils::CommandType::INFORMATION, *this, m_state));

    m_commands.emplace(
        NodeUtils::CommandType::QUERY,
        Command::Factory(NodeUtils::CommandType::QUERY, *this, m_state));

    m_commands.emplace(
        NodeUtils::CommandType::ELECTION,
        Command::Factory(NodeUtils::CommandType::ELECTION, *this, m_state));

    m_commands.emplace(
        NodeUtils::CommandType::TRANSFORM,
        Command::Factory(NodeUtils::CommandType::TRANSFORM, *this, m_state));

    m_commands.emplace(
        NodeUtils::CommandType::CONNECT,
        Command::Factory(NodeUtils::CommandType::CONNECT, *this, m_state));
    
    m_initialized = true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void CNode::Startup()
{
    if (m_initialized == false) {
        std::cout << "Node instance must be setup before starting!" << std::endl;
    }

    NodeUtils::DeviceOperation operation = NodeUtils::DeviceOperation::NONE;
    if (auto const selfState = m_state->GetSelfState().lock()) {
        operation = selfState->GetOperation();
    }

    if (operation == NodeUtils::DeviceOperation::NONE) {
        std::cout << "Node Setup must be provided and device operation type!" << std::endl;
    }

    printo("Starting up Brypt Node", NodeUtils::PrintType::NODE);

    srand(time(nullptr));

    switch (operation) {
        case NodeUtils::DeviceOperation::ROOT: {
            listen(); // Make threaded operation?
        } break;
        // Listen in thread?
        // Connect in another thread?
        // Bridge threads to receive upstream notifications and then pass down to own leafs
        // + pass aggregated messages to connect thread to respond with
        case NodeUtils::DeviceOperation::BRANCH: break;
        case NodeUtils::DeviceOperation::LEAF: {
            initialContact();  // Contact coordinator peer to get connection port
            connect(); // Make threaded operation?
        } break;
        case NodeUtils::DeviceOperation::NONE: break;
    }
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
std::shared_ptr<CConnection> CNode::SetupFullConnection(
    NodeUtils::NodeIdType const& peerId,
    std::string const& port,
    NodeUtils::TechnologyType const technology)
{
    NodeUtils::TOptions options;
    options.m_technology = technology;
    options.m_operation = NodeUtils::DeviceOperation::ROOT;
    options.m_port = port;
    options.m_isControl = false;
    options.m_peerName = peerId;

    m_queue->AddManagedPipe(options.m_peerName);

    return Connection::Factory(options);;
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
    return NodeUtils::TechnologyType::NONE;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
NodeUtils::TechnologyType CNode::determineBestConnectionType()
{
    auto const selfState = m_state->GetSelfState().lock();
    if (selfState) {
        return NodeUtils::TechnologyType::NONE;
    }
    // TODO: Actually determine the best technology type to use based on context
    auto const technologies = selfState->GetTechnologies();
    for (auto itr = technologies.begin(); itr != technologies.end(); ++itr) {
        switch (*itr) {
            case NodeUtils::TechnologyType::DIRECT:
            case NodeUtils::TechnologyType::STREAMBRIDGE:
            case NodeUtils::TechnologyType::TCP: return *itr;
            case NodeUtils::TechnologyType::LORA: return *itr;
            case NodeUtils::TechnologyType::NONE: return *itr;
        }
    }
    return NodeUtils::TechnologyType::NONE;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
bool CNode::hasTechnologyType(NodeUtils::TechnologyType const& technology)
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
    NodeUtils::NodeIdType id = std::string();
    NodeUtils::TOptions options;
    options.m_isControl = true;
    if (auto const selfState = m_state->GetSelfState().lock()) {
        id = selfState->GetId();
        options.m_technology = local::InitialContactTechnology(*selfState->GetTechnologies().begin());;
    } else { return; }

    char const* const technologyTypeStr = std::to_string(static_cast<std::uint32_t>(options.m_technology)).c_str();

    if (auto const coordinatorState = m_state->GetCoordinatorState().lock()) {
        options.m_peerAddress = coordinatorState->GetAddress();
        options.m_peerPort = coordinatorState->GetRequestPort();
    } else { return; }

    printo("Connecting with initial contact technology: " +
            std::to_string(static_cast<std::uint32_t>(options.m_technology)) +
            " and on addr:port: " + options.m_peerAddress + ":" + options.m_peerPort,
            NodeUtils::PrintType::NODE);

    std::shared_ptr<CConnection> const connection = Connection::Factory(options);

    std::optional<std::string> optResponse;
    printo("Sending coordinator acknowledgement", NodeUtils::PrintType::NODE);
    connection->Send("\x06");   // Send initial ACK byte to peer
    optResponse = connection->Receive(0);  // Expect ACK back from peer
    if (!optResponse) {
        return;
    }
    printo("Received: " + std::to_string(static_cast<std::int32_t>(optResponse->c_str()[0])) + "\n", NodeUtils::PrintType::NODE);

    // Send communication technology preferred
    printo("Sending preferred contact technology", NodeUtils::PrintType::NODE);
    connection->Send(technologyTypeStr);
    optResponse = connection->Receive(0);  // Expect new connection port from peer or something related to LoRa/Bluetooth
    if (!optResponse) {
        return;
    }

    CMessage portMessage(*optResponse);
    optResponse.reset();
    NodeUtils::NodeIdType coordinatorId = portMessage.GetData();
    if (auto const coordinatorState = m_state->GetCoordinatorState().lock()) {
        coordinatorState->SetId(portMessage.GetSourceId());
        coordinatorState->SetRequestPort(coordinatorId); // Set the coordinator port to the dedicated port
        printo("Port received: " + coordinatorId, NodeUtils::PrintType::NODE);
    } else { return; }

    printo("Sending node information", NodeUtils::PrintType::NODE);
    CMessage infoMessage(
        id, coordinatorId,
        NodeUtils::CommandType::CONNECT, 1,
        technologyTypeStr, 0);
    connection->Send(infoMessage);    // Send node information to peer

    optResponse = connection->Receive(0);  // Expect EOT back from peer
    printo("Received: " + std::to_string(static_cast<std::int32_t>(optResponse->c_str()[0])), NodeUtils::PrintType::NODE);
    printo("Connection sequence completed. Connecting to new endpoint", NodeUtils::PrintType::NODE);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    printo("Connection sequence completed. Shutting down initial connection", NodeUtils::PrintType::NODE);
    connection->Shutdown(); // Shutdown initial connection
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
void CNode::joinCoordinator()
{
    NodeUtils::TOptions options;
    options.m_operation = NodeUtils::DeviceOperation::LEAF;
    options.m_isControl = true;
    NodeUtils::PortNumber publisherPort  = std::string();
    if (auto const coordinatorState = m_state->GetCoordinatorState().lock()) {
        options.m_peerName = coordinatorState->GetId();
        options.m_peerAddress = coordinatorState->GetAddress();
        options.m_peerPort = coordinatorState->GetRequestPort();
        options.m_technology = coordinatorState->GetTechnology();
        publisherPort = coordinatorState->GetPublisherPort();
    } else { return; }

    printo("Connecting to Coordinator with technology: " + options.m_peerAddress + ":" + options.m_peerPort,
            NodeUtils::PrintType::NODE);

    m_notifier->Connect(options.m_peerAddress, publisherPort);
    m_connections->emplace(options.m_peerName, Connection::Factory(options));
    if (auto const networkState = m_state->GetNetworkState().lock()) {
        networkState->PushPeerName(options.m_peerName);
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
    printo("Handling request from control socket", NodeUtils::PrintType::NODE);

    if (message.empty()) {
        printo("No request to handle", NodeUtils::PrintType::NODE);
        return;
    }

    try {
        CMessage const request(message);
        if (auto const itr = m_commands.find(request.GetCommand()); itr != m_commands.end()) {
            itr->second->HandleMessage(request);
        }
    } catch(...) {
        printo("Control message failed to unpack", NodeUtils::PrintType::NODE);
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
    printo("Handling notification from coordinator", NodeUtils::PrintType::NODE);

    if (message.empty()) {
        printo("No notification to handle", NodeUtils::PrintType::NODE);
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
        printo("Control message failed to unpack", NodeUtils::PrintType::NODE);
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
    printo("Handling queue request from connection thread", NodeUtils::PrintType::NODE);
    NodeUtils::CommandType const& command = message.GetCommand();

    if (command == NodeUtils::CommandType::NONE) {
        printo("No command to handle", NodeUtils::PrintType::NODE);
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
    printo("Sending off fulfilled requests", NodeUtils::PrintType::NODE);

    if (m_awaiting->Empty()) {
        printo("No awaiting requests", NodeUtils::PrintType::NODE);
        return;
    }

    printo("Fulfilled requests:", NodeUtils::PrintType::NODE);
    std::vector<CMessage> responses = m_awaiting->GetFulfilled();

    // /*
    for (auto itr = responses.begin(); itr != responses.end(); ++itr) {
        m_queue->PushOutgoingMessage(itr->GetDestinationId(), *itr);
    }

    m_queue->PushOutgoingMessages();

    for (auto itr = responses.begin(); itr != responses.end(); ++itr) {
        this->NotifyConnection(itr->GetDestinationId());
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
    printo("Brypt Node is listening", NodeUtils::PrintType::NODE);
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

        m_queue->CheckPipes();
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
    printo("Brypt Node is connecting", NodeUtils::PrintType::NODE);
    joinCoordinator();
    printo("Joined coordinator", NodeUtils::PrintType::NODE);

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
NodeUtils::TechnologyType local::InitialContactTechnology(NodeUtils::TechnologyType const& technology)
{
    switch (technology) {
        case NodeUtils::TechnologyType::LORA:
        case NodeUtils::TechnologyType::NONE:
            return technology;
        case NodeUtils::TechnologyType::DIRECT:
        case NodeUtils::TechnologyType::TCP:
        case NodeUtils::TechnologyType::STREAMBRIDGE:
            return NodeUtils::TechnologyType::TCP;
    }
    return NodeUtils::TechnologyType::NONE;
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

    NodeUtils::NodeIdType id = std::string();
    if (auto const selfState = state->GetSelfState().lock()) {
        id = selfState->GetId();
    } else { return; }

    std::cout << "== [Node] Simulating client sensor Information request" << '\n';
    CMessage informationRequest("0xFFFFFFFF", id, NodeUtils::CommandType::INFORMATION, 0, "Request for Network Information.", 0);
    if (auto const itr = commands.find(informationRequest.GetCommand()); itr != commands.end()) {
        itr->second->HandleMessage(informationRequest);
    }
    
    std::cout << "== [Node] Simulating client sensor Query request" << '\n';
    CMessage queryRequest("0xFFFFFFFF",id, NodeUtils::CommandType::QUERY, 0, "Request for Sensor Readings.", 0);
    if (auto const itr = commands.find(queryRequest.GetCommand()); itr != commands.end()) {
        itr->second->HandleMessage(queryRequest);
    }
}

//------------------------------------------------------------------------------------------------
