//------------------------------------------------------------------------------------------------
// File: Information.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Information.hpp"
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/BryptNode.hpp"
#include "BryptNode/NodeState.hpp"
#include "BryptNode/CoordinatorState.hpp"
#include "BryptNode/NetworkState.hpp"
#include "Components/BryptPeer/PeerManager.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Utilities/LogUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------

std::string GenerateNodeInfo(BryptNode const& node);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace Json {
//------------------------------------------------------------------------------------------------

struct NodeInfo;

//------------------------------------------------------------------------------------------------
} // Json namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//------------------------------------------------------------------------------------------------
#ifndef LI_SYMBOL_identifier
#define LI_SYMBOL_identifier
LI_SYMBOL(identifier)
#endif
#ifndef LI_SYMBOL_cluster
#define LI_SYMBOL_cluster
LI_SYMBOL(cluster)
#endif
#ifndef LI_SYMBOL_coordinator
#define LI_SYMBOL_coordinator
LI_SYMBOL(coordinator)
#endif
#ifndef LI_SYMBOL_neighbor_count
#define LI_SYMBOL_neighbor_count
LI_SYMBOL(neighbor_count)
#endif
#ifndef LI_SYMBOL_designation
#define LI_SYMBOL_designation
LI_SYMBOL(designation)
#endif
#ifndef LI_SYMBOL_protocols
#define LI_SYMBOL_protocols
LI_SYMBOL(protocols)
#endif
#ifndef LI_SYMBOL_update_timestamp
#define LI_SYMBOL_update_timestamp
LI_SYMBOL(update_timestamp)
#endif
//------------------------------------------------------------------------------------------------

struct Json::NodeInfo
{
    NodeInfo(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        NodeUtils::ClusterIdType cluster,
        BryptIdentifier::SharedContainer const& spCoordinatorIdentifier,
        std::size_t neighbor_count,
        std::string const& designation,
        std::string const& protocols,
        TimeUtils::Timestamp const& update_timestamp)
        : identifier(spBryptIdentifier)
        , cluster(cluster)
        , coordinator(spCoordinatorIdentifier)
        , neighbor_count(neighbor_count)
        , designation(designation)
        , protocols(protocols)
        , update_timestamp(update_timestamp.count())
    {
    }
    BryptIdentifier::SharedContainer const identifier;
    NodeUtils::ClusterIdType const cluster;
    BryptIdentifier::SharedContainer const coordinator;
    std::size_t const neighbor_count;
    std::string const designation;
    std::string const protocols;
    std::uint64_t const update_timestamp;
};

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Handler::Information::Information(BryptNode& instance)
    : IHandler(Handler::Type::Information, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Information message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Information::HandleMessage(AssociatedMessage const& associatedMessage)
{
    bool status = false;

    auto& [wpBryptPeer, message] = associatedMessage;
    auto const phase = static_cast<Information::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::Flood: {
            status = FloodHandler(wpBryptPeer, message);
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
// Description: Handles the flood phase for the Information type handler
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Information::FloodHandler(
    std::weak_ptr<BryptPeer> const& wpBryptPeer, ApplicationMessage const& message)
{
    m_spLogger->debug(
        "Building response for the Information request from {}.",
        message.GetSourceIdentifier().GetNetworkRepresentation());

    IHandler::SendClusterNotice(
        wpBryptPeer, message,
        "Request for Node Information.",
        static_cast<std::uint8_t>(Phase::Respond),
        static_cast<std::uint8_t>(Phase::Close),
        local::GenerateNodeInfo(m_instance));

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the respond phase for the Information type handler
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Information::RespondHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the close phase for the Information type handler
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Information::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: This constructs a JSON object for each of the messages from each of the endpoints.
// Returns: The JSON structure as a string.
//------------------------------------------------------------------------------------------------
std::string local::GenerateNodeInfo(BryptNode const& instance)
{
    // Get the information pertaining to the node itself
    BryptIdentifier::SharedContainer spBryptIdentifier; 
    NodeUtils::ClusterIdType cluster = 0;
    NodeUtils::DeviceOperation operation = NodeUtils::DeviceOperation::None;
    if (auto const spNodeState = instance.GetNodeState().lock()) {
        spBryptIdentifier = spNodeState->GetBryptIdentifier();
        cluster = spNodeState->GetCluster();
        operation = spNodeState->GetOperation();
    }
    assert(spBryptIdentifier);

    // Get the information pertaining to the node's coordinator
    BryptIdentifier::SharedContainer spCoordinatorIdentifier; 
    if (auto const spCoordinatorState = instance.GetCoordinatorState().lock()) {
        spCoordinatorIdentifier = spCoordinatorState->GetBryptIdentifier();
    }

    // Get the information pertaining to the node's network
    std::uint32_t neighbors = 0;
    if (auto const spPeerManager = instance.GetPeerManager().lock(); spPeerManager) {
        neighbors = spPeerManager->ActivePeerCount();
    }

    std::vector<Json::NodeInfo> nodesInfo;
    nodesInfo.emplace_back(
        spBryptIdentifier, cluster, spCoordinatorIdentifier,
        neighbors, NodeUtils::GetDesignation(operation), 
        "IEEE 802.11", TimeUtils::GetSystemTimestamp());

    /* if (auto const spEndpoints = m_instance.GetEndpoints().lock(); spEndpoints) {
        for (auto const& endpoint : *spEndpoints) {
            // obtained through some other means. 
            
            std::string const timestamp = TimeUtils::TimepointToString(endpoint.GetUpdateClock());
            nodesInfo.emplace_back(
                spConnection->GetPeerName(), cluster, id, 0,
                "node", spConnection->GetScheme(), timestamp);
        }
    } */

    std::string const data = li::json_vector(
        s::identifier, s::cluster, s::coordinator, s::neighbor_count,
        s::designation, s::protocols, s::update_timestamp).encode(nodesInfo);

    return data;
}

//------------------------------------------------------------------------------------------------
