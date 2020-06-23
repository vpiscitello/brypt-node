//------------------------------------------------------------------------------------------------
// File: ConnectHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ConnectHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Endpoints/Endpoint.hpp"
#include "../Endpoints/EndpointManager.hpp"
#include "../MessageQueue/MessageQueue.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../BryptNode/NetworkState.hpp"
#include "../../BryptNode/NodeState.hpp"
#include "../../Configuration/PeerPersistor.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
#include <chrono>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

struct TTechnologyEntry
{
    std::string name;
    std::string entries;
};

using EntriesForTechnologiesVector = std::vector<TTechnologyEntry>;

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace Json {
//------------------------------------------------------------------------------------------------

struct TDiscoveryResponse;

//------------------------------------------------------------------------------------------------
} // Json namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//------------------------------------------------------------------------------------------------

IOD_SYMBOL(discovery_response)
// "discovery_response": {
//     "cluster": Number,
//     "technologies": [
//     {
//         "name": String
//         "entries": [Strings],
//     },
//     ...
//     ],
// }

IOD_SYMBOL(cluster)
IOD_SYMBOL(technologies)
IOD_SYMBOL(name)
IOD_SYMBOL(entries)
IOD_SYMBOL(entry)

//------------------------------------------------------------------------------------------------

struct Json::TDiscoveryResponse
{
    TDiscoveryResponse()
        : cluster()
        , technologies()
    {
    }

    TDiscoveryResponse(
        NodeUtils::ClusterIdType cluster,
        local::EntriesForTechnologiesVector const& technologies)
        : cluster(cluster)
        , technologies(technologies)
    {
    }

    NodeUtils::ClusterIdType cluster;
    local::EntriesForTechnologiesVector technologies;
};

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Command::CConnectHandler::CConnectHandler(CBryptNode& instance)
    : IHandler(Command::Type::Connect, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Connect message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::HandleMessage(CMessage const& message)
{
    // TODO: Implement mechanism to ensure all messages are verified before hitting 
    // handlers that require authenticated routes? 
    // If the message is not verified then drop the handling
    if (auto const status = message.Verify();
        status != Message::VerificationStatus::Success) {
        return false;
    }

    bool status = false;
    auto const phase = static_cast<CConnectHandler::Phase>(message.GetPhase());
    switch (phase) {
        // TODO: Implement method of authentication before supplying entries
        case Phase::Discovery: {
            status = DiscoveryHandler(message);
        } break;
        case Phase::Join: {
            status = JoinHandler(message);
        } break;
        default: break;
    }

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::DiscoveryHandler(CMessage const& message)
{
    // Make a response message to filled out by the handler
    Json::TDiscoveryResponse response;

    // Get the current Node ID and Cluster ID for this node
    NodeUtils::NodeIdType id;
    auto const wpNodeState = m_instance.GetNodeState();
    if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
        id = spNodeState->GetId();
        response.cluster = spNodeState->GetCluster();
    }

    using TechnologyEntriesMap = std::unordered_map<Endpoints::TechnologyType, std::vector<std::string>>;
    TechnologyEntriesMap mappedTechnologyEntries;
    // Get the current known peers of this node. The known peers will be supplied to the requestor
    // such that they may attempt to connect to them.
    auto const wpPeerPersistor = m_instance.GetPeerPersistor();
    // If the CPeerPersistor can be acquired from the node instance iterate through the known 
    // peers to obtain the entry addresses for the associated technologies. 
    if (auto const spPeerPersistor = wpPeerPersistor.lock(); spPeerPersistor) {
        // For each technology type stored in the cached peers emplace the entry into the asscoaited 
        // technology entries vector.
        spPeerPersistor->ForEachCachedPeer(
            // Get the entries for the requestor from the cached list of peers
            [&response, &mappedTechnologyEntries, &message] (Endpoints::TechnologyType technology, CPeer const& peer) -> CallbackIteration
            {
                if (peer.GetNodeId() != message.GetSourceId()) {
                    mappedTechnologyEntries[technology].emplace_back(peer.GetEntry());
                }

                return CallbackIteration::Continue;
            },
            // Unused cache error handling function
            [] ([[maybe_unused]] Endpoints::TechnologyType technology) {}
        );

        // Encode the peers list for the response
        for (auto const& [technology, entries]: mappedTechnologyEntries) {
            auto& technologyEntry = response.technologies.emplace_back();
            technologyEntry.name = Endpoints::TechnologyTypeToString(technology);
            // The metajson library can't encode nested vectors, so for now the entries are encoded 
            // before being placed in the data struct. 
            technologyEntry.entries = iod::json_encode(entries);
        }
    }

    std::string const encoded = iod::json_object(
        s::cluster,
        s::technologies = iod::json_vector(
            s::name,
            s::entries
        )).encode(response);

    // Using the information from the node instance generate a discovery response message
    CMessage responseMessage(
        message.GetMessageContext(),
        id, message.GetSourceId(),
        Command::Type::Connect, static_cast<std::uint8_t>(Phase::Join),
        encoded, 0);

    // Get the message queue from the node instance and forward the response
    auto const wpMessageQueue = m_instance.GetMessageQueue();
    if (auto const spMessageQueue = wpMessageQueue.lock(); spMessageQueue) {
        spMessageQueue->PushOutgoingMessage(responseMessage);
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the join phase for the Connect type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::JoinHandler(CMessage const& message)
{
    auto const buffer = message.GetData();
    auto const optDecryptedBuffer = message.Decrypt(buffer, buffer.size());
    if (!optDecryptedBuffer) {
        return false;
    }

    std::string const jsonResponse(optDecryptedBuffer->begin(), optDecryptedBuffer->end());

    // Parse response the response 
    Json::TDiscoveryResponse response;
    iod::json_object(
    s::cluster,
    s::technologies = iod::json_vector(
        s::name,
        s::entries
    )).decode(jsonResponse, response);

    auto wpEndpointManager = m_instance.GetEndpointManager();
    if (auto spEndpointManager = wpEndpointManager.lock(); spEndpointManager) {
        // The provided in the message should contain a series elements containing a technology name and 
        // vector of endpoint entries. We attempt to get the shared endpoint from the manager and then
        // schedule a connect event for each provided entry. The entry may or may not be address, it
        // is dependent on the attached technology. 
        for (auto const& [name, encoded]: response.technologies) {
            auto spEndpoint = spEndpointManager->GetEndpoint(
                message.GetMessageContext().GetEndpointTechnology(),
                Endpoints::OperationType::Client);

            if (!spEndpoint) {
                continue;
            }

            std::vector<std::string> entries;
            iod::json_decode(encoded, entries);

            for (auto const& entry: entries) {
                spEndpoint->ScheduleConnect(entry);
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------------------------