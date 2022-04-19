//----------------------------------------------------------------------------------------------------------------------
// File: Connect.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Connect.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptMessage/MessageContext.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Configuration/BootstrapService.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Action.hpp"
#include "Components/State/NodeState.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::string GenerateDiscoveryData(Configuration::Options::Endpoints const& endpoints);

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

bool Route::Fundamental::Connect::DiscoveryHandler::OnFetchServices(
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    m_wpNodeState = spServiceProvider->Fetch<NodeState>();
    if (m_wpNodeState.expired()) { return false; }

    m_wpBootstrapService = spServiceProvider->Fetch<BootstrapService>();
    if (m_wpBootstrapService.expired()) { return false; }

    m_wpNetworkManager = spServiceProvider->Fetch<Network::Manager>();
    if (m_wpNetworkManager.expired()) { return false; }
    
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryHandler::OnMessage(
    Message::Application::Parcel const& message, Peer::Action::Next& next)
{
    return HandleMessage(message, next) && BuildResponse(next);
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryHandler::HandleMessage(
    Message::Application::Parcel const& message, Peer::Action::Next& next)
{
    auto const spProxy = next.GetProxy().lock();
    if (!spProxy) { return false; }
    
    auto request = li::mmm(s::entrypoints = { li::mmm(s::protocol = std::string(), s::entry = std::string()) });
    auto const json = message.GetPayload().GetStringView();
    if (auto error = li::json_decode(json, request); error.bad()) { 
        m_logger->warn("Unable to decode discovery request from: {}", spProxy->GetIdentifier());
        return false;
    }

    if (!request.entrypoints.empty()) {
        // For each listed entrypoint, handle each entry for the given protocol.
        for (auto const& entrypoint: request.entrypoints) {
            // Parse the technology type from the human readable name.
            auto const protocol = Network::ParseProtocol(entrypoint.protocol);
            Network::RemoteAddress address(protocol, entrypoint.entry, true);
            if (!address.IsValid()) { m_logger->warn("Invalid boostrap received from: {}", spProxy->GetIdentifier()); }

            // Notify the BootstrapService of the entry for the protocol. By immediately storing the  entry it may be 
            // used in bootstrapping and distribution of entries for protocols to peers that have different capabilites 
            // not accessible by this node. The verification of entrypoints should be handled by a different module
            // (i.e. the endpoint or security mechanism).
            if (auto const spBootstrapService = m_wpBootstrapService.lock(); spBootstrapService) [[likely]] { 
                spBootstrapService->InsertBootstrap(address);
            }

            if (auto const spNetworkManager = m_wpNetworkManager.lock(); spNetworkManager) [[likely]] {
                // If we have an endpoint for the given protocol, schedule the connect.
                auto const spEndpoint = spNetworkManager->GetEndpoint(protocol, Network::Operation::Client); 
                if (spEndpoint) { spEndpoint->ScheduleConnect(std::move(address), spProxy->GetIdentifier()); }
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryHandler::BuildResponse(Peer::Action::Next& next)
{
    using BootstrapsMap = std::unordered_map<Network::Protocol, std::vector<std::string>>;

    // Make a response message to filled out by the handler
    auto payload = li::mmm(
        s::cluster = std::uint32_t(),
        s::bootstraps = { li::mmm(s::protocol = std::string(), s::entries = std::vector<std::string>()) });

    if (auto const spNodeState = m_wpNodeState.lock(); spNodeState) { payload.cluster = spNodeState->GetCluster(); }

    // If the BootstrapService can be acquired from the node instance iterate through the known  peers to obtain the entry
    //  addresses for the associated protocols. 
    BootstrapsMap bootstraps;
    if (auto const spBootstrapService = m_wpBootstrapService.lock(); spBootstrapService) {
        // For each protocol type stored in the cached peers emplace the entry into the associated protocol vector.
        spBootstrapService->ForEachBootstrap(
            // Get the entries for the requestor from the cached list of peers
            [&bootstraps] (Network::RemoteAddress const& bootstrap) -> CallbackIteration {
                bootstraps[bootstrap.GetProtocol()].emplace_back(bootstrap.GetUri());
                return CallbackIteration::Continue;
            }
        );

        // Encode the peers list for the response
        for (auto& [protocol, entries]: bootstraps) {
            auto& bootstrap = payload.bootstraps.emplace_back();
            bootstrap.protocol = Network::ProtocolToString(protocol);
            bootstrap.entries = std::move(entries);
        }
    }

    return next.Respond(li::json_encode(payload));
}

//----------------------------------------------------------------------------------------------------------------------

Route::Fundamental::Connect::DiscoveryProtocol::DiscoveryProtocol(
    Configuration::Options::Endpoints const& endpoints, std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
    : m_spNodeIdentifier()
    , m_wpNetworkManager(spServiceProvider->Fetch<Network::Manager>())
    , m_spSharedPayload(std::make_shared<std::string>(local::GenerateDiscoveryData(endpoints)))
    , m_logger(spdlog::get(Logger::Name::Core.data()))
{
    if (auto const spState = spServiceProvider->Fetch<NodeState>().lock(); spState) {
        m_spNodeIdentifier = spState->GetNodeIdentifier();
    }
    assert(m_spNodeIdentifier);
    assert(m_wpNetworkManager.lock() != nullptr);
    assert(m_logger);
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryProtocol::SendRequest(
    std::shared_ptr<Peer::Proxy> const& spProxy, Message::Context const& context) const
{
    assert(m_spSharedPayload && !m_spSharedPayload->empty());

    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(*m_spNodeIdentifier)
        .SetRoute(Route::Fundamental::Connect::DiscoveryHandler::Path)
        .SetPayload(m_spSharedPayload);

    return spProxy->Request(builder,
        [this, wpProxy = std::weak_ptr<Peer::Proxy>{ spProxy }] (auto const& response) {
            auto const spProxy = wpProxy.lock();
            if (!spProxy) { return; }

            auto payload = li::mmm(
                s::cluster = std::uint32_t(),
                s::bootstraps = { li::mmm(s::protocol = std::string(), s::entries = std::vector<std::string>()) });

            auto const json = response.GetPayload().GetStringView();
            if (auto error = li::json_decode(json, payload); error.bad()) { 
                m_logger->warn("Unable to decode discovery response from: {}", spProxy->GetIdentifier());
                return;
            }

            if (auto spNetworkManager = m_wpNetworkManager.lock(); spNetworkManager) {
                // The provided in the message should contain a series elements containing a protocol name and vector 
                // of endpoint entries. We attempt to get the shared endpoint from the  manager and then schedule a 
                // connect event for each provided entry. The entry may or may not be address, it is dependent on the 
                // attached protocol. 
                for (auto const& bootstrap: payload.bootstraps) {
                    auto const spEndpoint = spNetworkManager->GetEndpoint(
                        response.GetContext().GetEndpointProtocol(), Network::Operation::Client);
                    if (spEndpoint) [[likely]] {
                        auto const protocol = Network::ParseProtocol(bootstrap.protocol);
                        for (auto const& entry: bootstrap.entries) {
                            Network::RemoteAddress address(protocol, entry, true);
                            if (address.IsValid()) {
                                spEndpoint->ScheduleConnect(std::move(address));
                            } else {
                                m_logger->warn("Invalid boostrap received from: {}", spProxy->GetIdentifier());
                            }
                        }
                    }
                }
            }
        },
        [this] (Node::Identifier const& identifier, Peer::Action::Error) {
            m_logger->warn("Encountered an error waiting for discovery response from: {}", identifier);
        });
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::GenerateDiscoveryData(Configuration::Options::Endpoints const& endpoints)
{
    auto request = li::mmm(
        s::entrypoints = {
            li::mmm(s::protocol = std::string(), s::entry = std::string()) });

    request.entrypoints.clear();

    for (auto const& options: endpoints) {
        auto& entrypoint = request.entrypoints.emplace_back();
        entrypoint.protocol = options.GetProtocolString();
        entrypoint.entry = options.GetBinding().GetUri();
    }

    return li::json_encode(request);
}

//----------------------------------------------------------------------------------------------------------------------
