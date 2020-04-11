//------------------------------------------------------------------------------------------------
// File: InformationHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "InformationHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Await/Await.hpp"
#include "../Endpoints/Endpoint.hpp"
#include "../BryptNode/BryptNode.hpp"
#include "../BryptNode/NodeState.hpp"
#include "../BryptNode/CoordinatorState.hpp"
#include "../BryptNode/NetworkState.hpp"
#include "../Endpoints/Endpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------

std::string GenerateNodeInfo(CBryptNode const& node);

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
Command::CInformationHandler::CInformationHandler(CBryptNode& instance)
    : IHandler(Command::Type::Information, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Information message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformationHandler::HandleMessage(CMessage const& message)
{
    bool status = false;

    auto const phase = static_cast<CInformationHandler::Phase>(message.GetPhase());
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
bool Command::CInformationHandler::FloodHandler(CMessage const& message)
{
    printo("Building response for Information request", NodeUtils::PrintType::Command);
    
    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = 0;
    if (auto const spNodeState = m_instance.GetNodeState().lock()) {
        id = spNodeState->GetId();
    }

    NodeUtils::NetworkNonce const nonce = 0;

    if (auto const awaiting = m_instance.GetAwaiting().lock()) {
        auto const awaitKey = awaiting->PushRequest(message, id);

        CMessage const infoMessage(
            id,
            message.GetSourceId(),
            Command::Type::Information,
            static_cast<std::uint8_t>(Phase::Respond),
            local::GenerateNodeInfo(m_instance),
            nonce,
            Message::BoundAwaitId({Message::AwaitBinding::Destination, awaitKey}));

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
bool Command::CInformationHandler::RespondHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the close phase for the Information type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformationHandler::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: This constructs a JSON object for each of the messages from each of the endpoints.
// Returns: The JSON structure as a string.
//------------------------------------------------------------------------------------------------
std::string local::GenerateNodeInfo(CBryptNode const& instance)
{
    std::vector<Json::TNodeInfo> nodesInfo;

    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = 0; 
    NodeUtils::ClusterIdType cluster = 0;
    NodeUtils::DeviceOperation operation = NodeUtils::DeviceOperation::None;
    if (auto const spNodeState = instance.GetNodeState().lock()) {
        id = spNodeState->GetId();
        cluster = spNodeState->GetCluster();
        operation = spNodeState->GetOperation();
    }

    // Get the information pertaining to the node's coordinator
    NodeUtils::NodeIdType coordinatorId = 0; 
    if (auto const spCoordinatorState = instance.GetCoordinatorState().lock()) {
        coordinatorId = spCoordinatorState->GetId();
    }

    // Get the information pertaining to the node's network
    std::size_t knownNodes = 0;
    if (auto const spNetworkState = instance.GetNetworkState().lock()) {
        knownNodes = spNetworkState->GetKnownNodes();
    }

    nodesInfo.emplace_back(
        id, cluster, coordinatorId, knownNodes, NodeUtils::GetDesignation(operation), 
        "IEEE 802.11", NodeUtils::GetSystemTimestamp());

    // TODO: Endpoints represent a collection of peers, so the details of the peers must be 
    /* if (auto const spEndpoints = m_instance.GetEndpoints().lock(); spEndpoints) {
        for (auto const& endpoint : *spEndpoints) {
            // obtained through some other means. 
            
            std::string const timestamp = NodeUtils::TimepointToString(endpoint.GetUpdateClock());
            nodesInfo.emplace_back(
                spConnection->GetPeerName(),
                cluster,
                id,
                0,
                "node",
                spConnection->GetProtocolType(),
                timestamp);
        }
    } */

    std::string const data = iod::json_vector(
        s::id, s::cluster, s::coordinator, s::neighbor_count,
        s::designation, s::technologies, s::update_timestamp).encode(nodesInfo);

    return data;
}

//------------------------------------------------------------------------------------------------
