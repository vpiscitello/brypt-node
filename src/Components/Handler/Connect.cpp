//------------------------------------------------------------------------------------------------
// File: Connect.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Connect.hpp"
//------------------------------------------------------------------------------------------------
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/BryptNode.hpp"
#include "BryptNode/NetworkState.hpp"
#include "BryptNode/NodeState.hpp"
#include "Components/Configuration/PeerPersistor.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/EndpointManager.hpp"
//------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//------------------------------------------------------------------------------------------------
#include <chrono>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

bool HandleDiscoveryRequest(
    BryptNode& instance,
    std::weak_ptr<BryptPeer> const& wpBryptPeer,
    ApplicationMessage const& message);

std::string BuildDiscoveryResponse(BryptNode& instance);

bool HandleDiscoveryResponse(BryptNode& instance, ApplicationMessage const& message);

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
//         "protocol": String
//         "entry": String,
//     },
//     ...
//     ],
// }

// DiscoveryResponse {
//     "cluster": Number,
//     "bootstraps": [
//     {
//         "protocol": String
//         "entries": [Strings],
//     },
//     ...
//     ],
// }

#ifndef LI_SYMBOL_bootstraps
#define LI_SYMBOL_bootstraps
LI_SYMBOL(bootstraps)
#endif
#ifndef LI_SYMBOL_cluster
#define LI_SYMBOL_cluster
LI_SYMBOL(cluster)
#endif
#ifndef LI_SYMBOL_entries
#define LI_SYMBOL_entries
LI_SYMBOL(entries)
#endif
#ifndef LI_SYMBOL_entrypoints
#define LI_SYMBOL_entrypoints
LI_SYMBOL(entrypoints)
#endif
#ifndef LI_SYMBOL_entry
#define LI_SYMBOL_entry
LI_SYMBOL(entry)
#endif
#ifndef LI_SYMBOL_protocol
#define LI_SYMBOL_protocol
LI_SYMBOL(protocol)
#endif

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Handler::Connect::Connect(BryptNode& instance)
    : IHandler(Handler::Type::Connect, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Connect message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Connect::HandleMessage(AssociatedMessage const& associatedMessage)
{
    bool status = false;
    auto& [wpBryptPeer, message] = associatedMessage;
    auto const phase = static_cast<Connect::Phase>(message.GetPhase());
    switch (phase) {
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
bool Handler::Connect::DiscoveryHandler(
    std::weak_ptr<BryptPeer> const& wpBryptPeer,
    ApplicationMessage const& message)
{
    bool const status = local::HandleDiscoveryRequest(m_instance, wpBryptPeer, message);
    if (!status) { return status; }

    auto const response = local::BuildDiscoveryResponse(m_instance);
    IHandler::SendResponse(wpBryptPeer, message, response, static_cast<std::uint8_t>(Phase::Join));

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the join phase for the Connect type handler
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Handler::Connect::JoinHandler(ApplicationMessage const& message)
{
    return local::HandleDiscoveryResponse(m_instance, message);
}

//------------------------------------------------------------------------------------------------

bool local::HandleDiscoveryRequest(
    BryptNode& instance,
    std::weak_ptr<BryptPeer> const& wpBryptPeer,
    ApplicationMessage const& message)
{
    // Parse the discovery request
    auto const data = message.GetPayload();
    std::string_view const dataview(reinterpret_cast<char const*>(data.data()), data.size());
    
    auto request = li::mmm(
        s::entrypoints = {
            li::mmm(
                s::protocol = std::string(), s::entry = std::string()) });

    auto err = li::json_decode(dataview, request);

    BryptIdentifier::SharedContainer spPeerIdentifier;
    if (auto const spBryptPeer = wpBryptPeer.lock(); spBryptPeer) {
        spPeerIdentifier = spBryptPeer->GetBryptIdentifier();
    }

    if (!request.entrypoints.empty()) {
        // Get shared_ptrs for the PeerPersitor and EndpointManager
        auto spPeerPersistor = instance.GetPeerPersistor().lock();
        auto spEndpointManager = instance.GetEndpointManager().lock();
        
        // For each listed entrypoint, handle each entry for the given protocol.
        for (auto const& entrypoint: request.entrypoints) {
            // Parse the technoloy type from the human readible name.
            auto const protocol = Network::ParseProtocol(entrypoint.protocol);

            if (spPeerPersistor) {
                // Notify the PeerPersistor of the entry for the protocol. By immediately storing the 
                // entry it may be used in bootstrapping and distribution of entries for protocols
                // to peers that have different capabilites not accessible by this node. The verification
                // of entrypoints should be handled by a different module (i.e. the endpoint or security mechanism).
                spPeerPersistor->AddBootstrapEntry(protocol, entrypoint.entry);
            }

            if (spEndpointManager) {
                // If we have an endpoint for the given protocol, schedule the connect.
                if (auto spEndpoint = spEndpointManager->GetEndpoint(
                    protocol, Network::Operation::Client); spEndpoint) {
                    spEndpoint->ScheduleConnect(spPeerIdentifier, entrypoint.entry);
                }
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------------------------

std::string local::BuildDiscoveryResponse(BryptNode& instance)
{
    // Make a response message to filled out by the handler
    auto response = li::mmm(
        s::cluster = std::uint32_t(),
        s::bootstraps = {
            li::mmm(
                s::protocol = std::string(),
                s::entries = std::vector<std::string>()) });

    auto const wpNodeState = instance.GetNodeState();
    if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
        response.cluster = spNodeState->GetCluster();
    }

    using BootstrapsMap = std::unordered_map<Network::Protocol, std::vector<std::string>>;
    BootstrapsMap bootstraps;
    // Get the current known peers of this node. The known peers will be supplied to the requestor
    // such that they may attempt to connect to them.
    auto const wpPeerPersistor = instance.GetPeerPersistor();
    // If the PeerPersistor can be acquired from the node instance iterate through the known 
    // peers to obtain the entry addresses for the associated protocols. 
    if (auto const spPeerPersistor = wpPeerPersistor.lock(); spPeerPersistor) {
        // For each protocol type stored in the cached peers emplace the entry into the asscoaited 
        // protocol entries vector.
        spPeerPersistor->ForEachCachedBootstrap(
            // Get the entries for the requestor from the cached list of peers
            [&bootstraps] (
                Network::Protocol protocol,
                std::string_view const& bootstrap) -> CallbackIteration
            {
                bootstraps[protocol].emplace_back(bootstrap);
                return CallbackIteration::Continue;
            },
            // Unused cache error handling function
            [] ([[maybe_unused]] Network::Protocol protocol) {}
        );

        // Encode the peers list for the response
        for (auto& [protocol, entries]: bootstraps) {
            auto& bootstrap = response.bootstraps.emplace_back();
            bootstrap.protocol = Network::ProtocolToString(protocol);
            bootstrap.entries = std::move(entries);
        }
    }

    return li::json_encode(response);
}

//------------------------------------------------------------------------------------------------

bool local::HandleDiscoveryResponse(BryptNode& instance, ApplicationMessage const& message)
{
    // Parse the discovery response
    auto const data = message.GetPayload();
    std::string_view const dataview(reinterpret_cast<char const*>(data.data()), data.size());

    auto response = li::mmm(
        s::cluster = std::uint32_t(),
        s::bootstraps = {
            li::mmm(
                s::protocol = std::string(),
                s::entries = std::vector<std::string>()) });

    auto err = li::json_decode(dataview, response);

    auto wpEndpointManager = instance.GetEndpointManager();
    if (auto spEndpointManager = wpEndpointManager.lock(); spEndpointManager) {
        // The provided in the message should contain a series elements containing a protocol 
        // name and vector of endpoint entries. We attempt to get the shared endpoint from the 
        // manager and then schedule a connect event for each provided entry. The entry may or
        //  may not be address, it is dependent on the attached protocol. 
        for (auto const& bootstrap: response.bootstraps) {
            auto spEndpoint = spEndpointManager->GetEndpoint(
                message.GetContext().GetEndpointProtocol(),
                Network::Operation::Client);

            if (spEndpoint) {
                for (auto const& entry: bootstrap.entries) { spEndpoint->ScheduleConnect(entry); }
            }
        }
    }

    return true;
}

//------------------------------------------------------------------------------------------------
