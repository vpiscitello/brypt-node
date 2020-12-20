//------------------------------------------------------------------------------------------------
// File: InformationHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "InformationHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Endpoints/Endpoint.hpp"
#include "../BryptPeer/PeerManager.hpp"
#include "../../BryptIdentifier/BryptIdentifier.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../BryptNode/NodeState.hpp"
#include "../../BryptNode/CoordinatorState.hpp"
#include "../../BryptNode/NetworkState.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
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
#ifndef LI_SYMBOL_technologies
#define LI_SYMBOL_technologies
LI_SYMBOL(technologies)
#endif
#ifndef LI_SYMBOL_update_timestamp
#define LI_SYMBOL_update_timestamp
LI_SYMBOL(update_timestamp)
#endif
//------------------------------------------------------------------------------------------------

struct Json::TNodeInfo
{
    TNodeInfo(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        NodeUtils::ClusterIdType cluster,
        BryptIdentifier::SharedContainer const& spCoordinatorIdentifier,
        std::size_t neighbor_count,
        std::string const& designation,
        std::string const& technologies,
        TimeUtils::Timestamp const& update_timestamp)
        : identifier(spBryptIdentifier)
        , cluster(cluster)
        , coordinator(spCoordinatorIdentifier)
        , neighbor_count(neighbor_count)
        , designation(designation)
        , technologies(technologies)
        , update_timestamp(update_timestamp.count())
    {
    }
    BryptIdentifier::SharedContainer const identifier;
    NodeUtils::ClusterIdType const cluster;
    BryptIdentifier::SharedContainer const coordinator;
    std::size_t const neighbor_count;
    std::string const designation;
    std::string const technologies;
    std::uint64_t const update_timestamp;
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
bool Command::CInformationHandler::HandleMessage(AssociatedMessage const& associatedMessage)
{
    bool status = false;

    auto& [wpBryptPeer, message] = associatedMessage;
    auto const phase = static_cast<CInformationHandler::Phase>(message.GetPhase());
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
// Description: Handles the flood phase for the Information type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformationHandler::FloodHandler(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer, CApplicationMessage const& message)
{
    printo("Building response for Information request", NodeUtils::PrintType::Command);
    
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

    std::vector<Json::TNodeInfo> nodesInfo;
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
                "node", spConnection->GetProtocolType(), timestamp);
        }
    } */

    std::string const data = li::json_vector(
        s::identifier, s::cluster, s::coordinator, s::neighbor_count,
        s::designation, s::technologies, s::update_timestamp).encode(nodesInfo);

    return data;
}

//------------------------------------------------------------------------------------------------
