//------------------------------------------------------------------------------------------------
// File: ConnectHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ConnectHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Endpoints/Endpoint.hpp"
#include "../Endpoints/EndpointManager.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../BryptNode/NetworkState.hpp"
#include "../../BryptNode/NodeState.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
//------------------------------------------------------------------------------------------------
#include <chrono>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

bool HandleDiscoveryRequest(CBryptNode& instance, CApplicationMessage const& message);
std::string BuildDiscoveryResponse(CBryptNode& instance);

bool HandleDiscoveryResponse(CBryptNode& instance, CApplicationMessage const& message);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//------------------------------------------------------------------------------------------------

// DiscoveryRequest {
//     "entrypoints": [
//     {
//         "technology": String
//         "entry": String,
//     },
//     ...
//     ],
// }

// DiscoveryResponse {
//     "cluster": Number,
//     "bootstraps": [
//     {
//         "technology": String
//         "entries": [Strings],
//     },
//     ...
//     ],
// }

IOD_SYMBOL(bootstraps)
IOD_SYMBOL(cluster)
IOD_SYMBOL(entries)
IOD_SYMBOL(entry)
IOD_SYMBOL(entrypoints)
IOD_SYMBOL(technology)

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
bool Command::CConnectHandler::HandleMessage(AssociatedMessage const& associatedMessage)
{
    bool status = false;
    auto& [wpBryptPeer, message] = associatedMessage;
    auto const phase = static_cast<CConnectHandler::Phase>(message.GetPhase());
    switch (phase) {
        // TODO: Implement method of authentication before supplying entries
        case Phase::Discovery: {
            status = DiscoveryHandler(wpBryptPeer, message);
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
bool Command::CConnectHandler::DiscoveryHandler(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CApplicationMessage const& message)
{
    bool const status = local::HandleDiscoveryRequest(m_instance, message);
    if (!status) {
        return status;
    }

    auto const response = local::BuildDiscoveryResponse(m_instance);
    IHandler::SendResponse(wpBryptPeer, message, response, static_cast<std::uint8_t>(Phase::Join));

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the join phase for the Connect type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::JoinHandler(CApplicationMessage const& message)
{
    return local::HandleDiscoveryResponse(m_instance, message);
}

//------------------------------------------------------------------------------------------------

bool local::HandleDiscoveryRequest(CBryptNode& instance, CApplicationMessage const& message)
{
    // Parse the discovery request
    auto const data = message.GetPayload();
    std::string_view const dataview(reinterpret_cast<char const*>(data.data()), data.size());

    auto request = iod::make_metamap(
        s::entrypoints = {
            iod::make_metamap(
                s::technology = std::string(), s::entry = std::string()) });

    iod::json_decode(dataview, request);

    if (!request.entrypoints.empty()) {
        // Get shared_ptrs for the PeerPersitor and EndpointManager
        auto spPeerPersistor = instance.GetPeerPersistor().lock();
        auto spEndpointManager = instance.GetEndpointManager().lock();
        
        // For each listed entrypoint, handle each entry for the given technology.
        for (auto const& entrypoint: request.entrypoints) {
            // Parse the technoloy type from the human readible name.
            auto const technology = Endpoints::ParseTechnologyType(entrypoint.technology);

            if (spPeerPersistor) {
                // Notify the PeerPersistor of the entry for the technology. By immediately storing the 
                // entry it may be used in bootstrapping and distribution of entries for technologies
                // to peers that have different capabilites not accessible by this node. The verification
                // of entrypoints should be handled by a different module (i.e. the endpoint or security mechanism).
                spPeerPersistor->AddBootstrapEntry(technology, entrypoint.entry);
            }

            if (spEndpointManager) {
                // If we have an endpoint for the given technology, schedule the connect.
                if (auto spEndpoint = spEndpointManager->GetEndpoint(
                    technology, Endpoints::OperationType::Client); spEndpoint) {
                    spEndpoint->ScheduleConnect(entrypoint.entry);
                }
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------------------------

std::string local::BuildDiscoveryResponse(CBryptNode& instance)
{
    // Make a response message to filled out by the handler
    auto response = iod::make_metamap(
        s::cluster = std::uint32_t(),
        s::bootstraps = {
            iod::make_metamap(
                s::technology = std::string(),
                s::entries = std::vector<std::string>()) });

    auto const wpNodeState = instance.GetNodeState();
    if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
        response.cluster = spNodeState->GetCluster();
    }

    using BootstrapsMap = std::unordered_map<Endpoints::TechnologyType, std::vector<std::string>>;
    BootstrapsMap bootstraps;
    // Get the current known peers of this node. The known peers will be supplied to the requestor
    // such that they may attempt to connect to them.
    auto const wpPeerPersistor = instance.GetPeerPersistor();
    // If the CPeerPersistor can be acquired from the node instance iterate through the known 
    // peers to obtain the entry addresses for the associated technologies. 
    if (auto const spPeerPersistor = wpPeerPersistor.lock(); spPeerPersistor) {
        // For each technology type stored in the cached peers emplace the entry into the asscoaited 
        // technology entries vector.
        spPeerPersistor->ForEachCachedBootstrap(
            // Get the entries for the requestor from the cached list of peers
            [&bootstraps] (
                Endpoints::TechnologyType technology,
                std::string_view const& bootstrap) -> CallbackIteration
            {
                bootstraps[technology].emplace_back(bootstrap);
                return CallbackIteration::Continue;
            },
            // Unused cache error handling function
            [] ([[maybe_unused]] Endpoints::TechnologyType technology) {}
        );

        // Encode the peers list for the response
        for (auto& [technology, entries]: bootstraps) {
            auto& bootstrap = response.bootstraps.emplace_back();
            bootstrap.technology = Endpoints::TechnologyTypeToString(technology);
            bootstrap.entries = std::move(entries);
        }
    }

    return iod::json_encode(response);
}

//------------------------------------------------------------------------------------------------

bool local::HandleDiscoveryResponse(CBryptNode& instance, CApplicationMessage const& message)
{
    // Parse the discovery response
    auto const data = message.GetPayload();
    std::string_view const dataview(reinterpret_cast<char const*>(data.data()), data.size());

    auto response = iod::make_metamap(
        s::cluster = std::uint32_t(),
        s::bootstraps = {
            iod::make_metamap(
                s::technology = std::string(),
                s::entries = std::vector<std::string>()) });

    iod::json_decode(dataview, response);

    auto wpEndpointManager = instance.GetEndpointManager();
    if (auto spEndpointManager = wpEndpointManager.lock(); spEndpointManager) {
        // The provided in the message should contain a series elements containing a technology 
        // name and vector of endpoint entries. We attempt to get the shared endpoint from the 
        // manager and then schedule a connect event for each provided entry. The entry may or
        //  may not be address, it is dependent on the attached technology. 
        for (auto const& bootstrap: response.bootstraps) {
            auto spEndpoint = spEndpointManager->GetEndpoint(
                message.GetMessageContext().GetEndpointTechnology(),
                Endpoints::OperationType::Client);

            if (spEndpoint) {
                for (auto const& entry: bootstrap.entries) {
                    spEndpoint->ScheduleConnect(entry);
                }
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------------------------
