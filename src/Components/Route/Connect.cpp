//----------------------------------------------------------------------------------------------------------------------
// File: Connect.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Connect.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/BryptNode.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/State/NodeState.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <chrono>
#include <thread>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

bool HandleDiscoveryRequest(
    Node::Core& instance,
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& message,
    std::shared_ptr<spdlog::logger> const& logger);

std::string BuildDiscoveryResponse(Node::Core& instance);

bool HandleDiscoveryResponse(
    Node::Core& instance,
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& message,
    std::shared_ptr<spdlog::logger> const& logger);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//----------------------------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
Handler::Connect::Connect(Node::Core& instance)
    : IHandler(Handler::Type::Connect, instance)
{
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Connect message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//----------------------------------------------------------------------------------------------------------------------
bool Handler::Connect::HandleMessage(AssociatedMessage const& associatedMessage)
{
    bool status = false;

    // auto& [wpPeerProxy, message] = associatedMessage;
    // auto const phase = static_cast<Connect::Phase>(message.GetPhase());
    // switch (phase) {
    //     case Phase::Discovery: { status = DiscoveryHandler(wpPeerProxy, message); } break;
    //     case Phase::Join: { status = JoinHandler(wpPeerProxy, message); } break;
    //     default: break;
    // }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//----------------------------------------------------------------------------------------------------------------------
bool Handler::Connect::DiscoveryHandler(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage const& message)
{
    // bool const status = local::HandleDiscoveryRequest(
    //     m_instance, wpPeerProxy, message, m_logger);
    // if (!status) { return status; }

    // auto const response = local::BuildDiscoveryResponse(m_instance);
    // IHandler::SendResponse(wpPeerProxy, message, response, static_cast<std::uint8_t>(Phase::Join));

    // return status;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Handles the join phase for the Connect type handler
// Returns: Status of the message handling
//----------------------------------------------------------------------------------------------------------------------
bool Handler::Connect::JoinHandler(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy, ApplicationMessage const& message)
{
    // return local::HandleDiscoveryResponse(m_instance, wpPeerProxy, message, m_logger);
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::HandleDiscoveryRequest(
    Node::Core& instance,
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& message,
    std::shared_ptr<spdlog::logger> const& logger)
{
    // Node::SharedIdentifier spPeerIdentifier;
    // if (auto const spPeerProxy = wpPeerProxy.lock(); spPeerProxy) {
    //     spPeerIdentifier = spPeerProxy->GetIdentifier();
    // }

    // // Parse the discovery request
    // auto const data = message.GetPayload();
    // std::string_view const dataview(reinterpret_cast<char const*>(data.data()), data.size());
    
    // auto request = li::mmm(
    //     s::entrypoints = {
    //         li::mmm(
    //             s::protocol = std::string(), s::entry = std::string()) });

    // if (auto error = li::json_decode(dataview, request); error.bad()) { 
    //     logger->warn("Unable to decode discovery request from: {}", spPeerIdentifier);
    //     return false;
    // }

    // if (!request.entrypoints.empty()) {
    //     auto spBootstrapService = instance.GetBootstrapService().lock();
    //     auto spNetworkManager = instance.GetNetworkManager().lock();
        
    //     // For each listed entrypoint, handle each entry for the given protocol.
    //     for (auto const& entrypoint: request.entrypoints) {
    //         // Parse the technoloy type from the human readible name.
    //         auto const protocol = Network::ParseProtocol(entrypoint.protocol);
    //         Network::RemoteAddress address(protocol, entrypoint.entry, true);
    //         if (!address.IsValid()) { logger->warn("Invalid boostrap received from: {}", spPeerIdentifier); }

    //         // Notify the BootstrapService of the entry for the protocol. By immediately 
    //         // storing the  entry it may be used in bootstrapping and distribution of entries
    //         // for protocols to peers that have different capabilites not accessible by this 
    //         // node. The verification of entrypoints should be handled by a different module
    //         //  (i.e. the endpoint or security mechanism).
    //         if (spBootstrapService) [[likely]] { spBootstrapService->InsertBootstrap(address); }

    //         if (spNetworkManager) [[likely]] {
    //             // If we have an endpoint for the given protocol, schedule the connect.
    //             if (auto const spEndpoint = spNetworkManager->GetEndpoint(protocol, Network::Operation::Client);
    //                 spEndpoint) {
    //                 spEndpoint->ScheduleConnect(std::move(address), spPeerIdentifier);
    //             }
    //         }
    //     }
    // }

    // return true;
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::BuildDiscoveryResponse(Node::Core& instance)
{
    // // Make a response message to filled out by the handler
    // auto response = li::mmm(
    //     s::cluster = std::uint32_t(),
    //     s::bootstraps = {
    //         li::mmm(
    //             s::protocol = std::string(),
    //             s::entries = std::vector<std::string>()) });

    // auto const wpNodeState = instance.GetNodeState();
    // if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
    //     response.cluster = spNodeState->GetCluster();
    // }

    // using BootstrapsMap = std::unordered_map<Network::Protocol, std::vector<std::string>>;
    // BootstrapsMap bootstraps;
    // // Get the current known peers of this node. The known peers will be supplied to the requestor
    // // such that they may attempt to connect to them.
    // auto const wpBootstrapService = instance.GetBootstrapService();
    // // If the BootstrapService can be acquired from the node instance iterate through the known 
    // // peers to obtain the entry addresses for the associated protocols. 
    // if (auto const spBootstrapService = wpBootstrapService.lock(); spBootstrapService) {
    //     // For each protocol type stored in the cached peers emplace the entry into the asscoaited 
    //     // protocol entries vector.
    //     spBootstrapService->ForEachBootstrap(
    //         // Get the entries for the requestor from the cached list of peers
    //         [&bootstraps] (Network::RemoteAddress const& bootstrap) -> CallbackIteration
    //         {
    //             bootstraps[bootstrap.GetProtocol()].emplace_back(bootstrap.GetUri());
    //             return CallbackIteration::Continue;
    //         }
    //     );

    //     // Encode the peers list for the response
    //     for (auto& [protocol, entries]: bootstraps) {
    //         auto& bootstrap = response.bootstraps.emplace_back();
    //         bootstrap.protocol = Network::ProtocolToString(protocol);
    //         bootstrap.entries = std::move(entries);
    //     }
    // }

    // return li::json_encode(response);
    return "";
}

//----------------------------------------------------------------------------------------------------------------------

bool local::HandleDiscoveryResponse(
    Node::Core& instance,
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& message,
    std::shared_ptr<spdlog::logger> const& logger)
{
    // Node::SharedIdentifier spPeerIdentifier;
    // if (auto const spPeerProxy = wpPeerProxy.lock(); spPeerProxy) {
    //     spPeerIdentifier = spPeerProxy->GetIdentifier();
    // }

    // // Parse the discovery response
    // auto const data = message.GetPayload();
    // std::string_view const dataview(reinterpret_cast<char const*>(data.data()), data.size());

    // auto response = li::mmm(
    //     s::cluster = std::uint32_t(),
    //     s::bootstraps = {
    //         li::mmm(
    //             s::protocol = std::string(),
    //             s::entries = std::vector<std::string>()) });

    // if (auto error = li::json_decode(dataview, response); error.bad()) { 
    //     logger->warn("Unable to decode discovery response from: {}", spPeerIdentifier);
    //     return false;
    // }

    // if (auto spManager = instance.GetNetworkManager().lock(); spManager) {
    //     // The provided in the message should contain a series elements containing a protocol 
    //     // name and vector of endpoint entries. We attempt to get the shared endpoint from the 
    //     // manager and then schedule a connect event for each provided entry. The entry may or
    //     //  may not be address, it is dependent on the attached protocol. 
    //     for (auto const& bootstrap: response.bootstraps) {
    //         auto spEndpoint = spManager->GetEndpoint(
    //             message.GetContext().GetEndpointProtocol(), Network::Operation::Client);
    //         if (spEndpoint) [[likely]] {
    //             auto const protocol = Network::ParseProtocol(bootstrap.protocol);
    //             for (auto const& entry: bootstrap.entries) {
    //                 Network::RemoteAddress address(protocol, entry, true);
    //                 if (address.IsValid()) {
    //                     spEndpoint->ScheduleConnect(std::move(address));
    //                 } else {
    //                     logger->warn("Invalid boostrap received from: {}", spPeerIdentifier);
    //                 }
    //             }
    //         }
    //     }
    // }

    // return true;
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
