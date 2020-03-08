//------------------------------------------------------------------------------------------------
// File: InformationHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "InformationHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Await/Await.hpp"
#include "../Connection/Connection.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------

std::string GenerateNodeInfo(CNode const& node, std::weak_ptr<CState> const& state);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace Json {
//------------------------------------------------------------------------------------------------

struct TNodeInfo;

//------------------------------------------------------------------------------------------------
} // Json namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//------------------------------------------------------------------------------------------------
IOD_SYMBOL(id)
IOD_SYMBOL(cluster)
IOD_SYMBOL(coordinator)
IOD_SYMBOL(neighbor_count)
IOD_SYMBOL(designation)
IOD_SYMBOL(technologies)
IOD_SYMBOL(update_timestamp)
//------------------------------------------------------------------------------------------------

struct Json::TNodeInfo
{
    TNodeInfo(
        NodeUtils::NodeIdType id,
        NodeUtils::ClusterIdType cluster,
        NodeUtils::NodeIdType coordinator,
        std::size_t neighbor_count,
        std::string const& designation,
        std::string const& technologies,
        std::string const& update_timestamp)
        : id(id)
        , cluster(cluster)
        , coordinator(coordinator)
        , neighbor_count(neighbor_count)
        , designation(designation)
        , technologies(technologies)
        , update_timestamp(update_timestamp)
    {
    }
    NodeUtils::NodeIdType const id;
    NodeUtils::ClusterIdType const cluster;
    NodeUtils::NodeIdType const coordinator;
    std::size_t const neighbor_count;
    std::string const designation;
    std::string const technologies;
    std::string const update_timestamp;
};

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Command::CInformation::CInformation(CNode& instance, std::weak_ptr<CState> const& state)
    : CHandler(instance, state, NodeUtils::CommandType::Information)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Information message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformation::HandleMessage(CMessage const& message)
{
    bool status = false;

    auto const phase = static_cast<CInformation::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::Flood: {
            status = FloodHandler(message);
        } break;
        case Phase::Respond: {
            status = RespondHandler();
        } break;
        case Phase::Close: {
            status = CloseHandler();
        } break;
        default: break;
    }

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the flood phase for the Information type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformation::FloodHandler(CMessage const& message)
{
    printo("Building response for Information request", NodeUtils::PrintType::Command);
    
    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = 0; 
    if (auto const selfState = m_state.lock()->GetSelfState().lock()) {
        id = selfState->GetId();
    }

    NodeUtils::NetworkNonce const nonce = 0;

    if (auto const awaiting = m_instance.GetAwaiting().lock()) {
        auto const awaitKey = awaiting->PushRequest(message, id);

        CMessage const infoMessage(
            id, message.GetSourceId(),
            NodeUtils::CommandType::Information,
            static_cast<std::uint8_t>(Phase::Respond),
            local::GenerateNodeInfo(m_instance, m_state), nonce,
            Message::BoundAwaitId(
                {Message::AwaitBinding::Destination, awaitKey}));

        awaiting->PushResponse(infoMessage);
    }

    // TODO: Add notification distribution, so branches can add their information

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the respond phase for the Information type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformation::RespondHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the close phase for the Information type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformation::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: This constructs a JSON object for each of the messages from each of the connections.
// Returns: The JSON structure as a string.
//------------------------------------------------------------------------------------------------
std::string local::GenerateNodeInfo(CNode const& m_instance, std::weak_ptr<CState> const& state)
{
    std::vector<Json::TNodeInfo> nodesInfo;

    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = 0; 
    NodeUtils::ClusterIdType cluster = 0;
    NodeUtils::DeviceOperation operation = NodeUtils::DeviceOperation::None;
    if (auto const selfState = state.lock()->GetSelfState().lock()) {
        id = selfState->GetId();
        cluster = selfState->GetCluster();
        operation = selfState->GetOperation();
    }

    // Get the information pertaining to the node's coordinator
    NodeUtils::NodeIdType coordinatorId = 0; 
    if (auto const coordinatorState = state.lock()->GetCoordinatorState().lock()) {
        coordinatorId = coordinatorState->GetId();
    }

    // Get the information pertaining to the node's network
    std::size_t knownNodes = 0;
    if (auto const networkState = state.lock()->GetNetworkState().lock()) {
        knownNodes = networkState->GetKnownNodes();
    }

    nodesInfo.emplace_back(
        id, cluster, coordinatorId, knownNodes, NodeUtils::GetDesignation(operation), 
        "IEEE 802.11", NodeUtils::GetSystemTimestamp());

    if (auto const connections = m_instance.GetConnections().lock()) {
        for (auto itr = connections->begin(); itr != connections->end(); ++itr) {
            std::string const timestamp = NodeUtils::TimePointToString(itr->second->GetUpdateClock());
            nodesInfo.emplace_back(
                itr->second->GetPeerName(), cluster, id, 0, "node",
                itr->second->GetProtocolType(), timestamp);
        }
    }

    std::string data = iod::json_vector(
        s::id, s::cluster, s::coordinator, s::neighbor_count,
        s::designation, s::technologies, s::update_timestamp).encode(nodesInfo);

    return data;
}

//------------------------------------------------------------------------------------------------
