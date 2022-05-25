//----------------------------------------------------------------------------------------------------------------------
// File: Information.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Information.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/ServiceProvider.hpp"
#include "Components/Awaitable/TrackingService.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Components/Network/Manager.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Action.hpp"
#include "Components/State/CoordinatorState.hpp"
#include "Components/State/NetworkState.hpp"
#include "Components/State/NodeState.hpp"
#include "Interfaces/PeerCache.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
//----------------------------------------------------------------------------------------------------------------------
namespace Json {
//----------------------------------------------------------------------------------------------------------------------

std::string GenerateNodeInfo(
    std::weak_ptr<NodeState> const& wpNodeState,
    std::weak_ptr<NetworkState> const& wpNetworkState,
    std::weak_ptr<Network::Manager> const& wpNetworkManager,
    std::weak_ptr<IPeerCache> const& wpPeerCache);

//----------------------------------------------------------------------------------------------------------------------
} // Json namespace
//----------------------------------------------------------------------------------------------------------------------
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//----------------------------------------------------------------------------------------------------------------------
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
//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Information::NodeHandler::OnFetchServices(
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    m_wpNodeState = spServiceProvider->Fetch<NodeState>();
    if (m_wpNodeState.expired()) { return false; }

    m_wpCoordinatorState = spServiceProvider->Fetch<CoordinatorState>();
    if (m_wpCoordinatorState.expired()) { return false; }
    
    m_wpNetworkState = spServiceProvider->Fetch<NetworkState>();
    if (m_wpNetworkState.expired()) { return false; }
    
    m_wpNetworkManager = spServiceProvider->Fetch<Network::Manager>();
    if (m_wpNetworkManager.expired()) { return false; }
    
    m_wpPeerCache = spServiceProvider->Fetch<IPeerCache>();
    if (m_wpPeerCache.expired()) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Information::NodeHandler::OnMessage(
     Message::Application::Parcel const& message, Peer::Action::Next& next)
{
    using namespace Message;
    auto const optAwaitable = message.GetExtension<Extension::Awaitable>(); 
    if (!optAwaitable || optAwaitable->get().GetBinding() != Extension::Awaitable::Request) { return false; }

    return next.Respond(
        Json::GenerateNodeInfo(m_wpNodeState, m_wpNetworkState, m_wpNetworkManager, m_wpPeerCache));
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Information::FetchNodeHandler::OnFetchServices(
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    m_wpNodeState = spServiceProvider->Fetch<NodeState>();
    if (m_wpNodeState.expired()) { return false; }

    m_wpCoordinatorState = spServiceProvider->Fetch<CoordinatorState>();
    if (m_wpCoordinatorState.expired()) { return false; }
    
    m_wpNetworkState = spServiceProvider->Fetch<NetworkState>();
    if (m_wpNetworkState.expired()) { return false; }
    
    m_wpNetworkManager = spServiceProvider->Fetch<Network::Manager>();
    if (m_wpNetworkManager.expired()) { return false; }
    
    m_wpPeerCache = spServiceProvider->Fetch<IPeerCache>();
    if (m_wpPeerCache.expired()) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Information::FetchNodeHandler::OnMessage(
    Message::Application::Parcel const& message, Peer::Action::Next& next)
{
    m_logger->debug("Handling a cluster information request from {}.", message.GetSource());

    auto const optTrackerKey = next.Defer({
        .notice = {
            .type = Message::Destination::Cluster,
            .route = NodeHandler::Path
        },
        .response = {
            .payload = Json::GenerateNodeInfo(m_wpNodeState, m_wpNetworkState, m_wpNetworkManager, m_wpPeerCache)
        }
    });

    return optTrackerKey.has_value();
}

//----------------------------------------------------------------------------------------------------------------------

std::string Json::GenerateNodeInfo(
    std::weak_ptr<NodeState> const& wpNodeState,
    std::weak_ptr<NetworkState> const& wpNetworkState,
    std::weak_ptr<Network::Manager> const& wpNetworkManager,
    std::weak_ptr<IPeerCache> const& wpPeerCache)
{
    // Make a response message to filled out by the handler
    auto payload = li::mmm(
        s::cluster = std::uint32_t(),
        s::neighbor_count = std::uint32_t(),
        s::designation = std::string(),
        s::protocols = std::vector<std::string>(),
        s::update_timestamp = std::uint64_t());

    if (auto const spNodeState = wpNodeState.lock(); spNodeState) { 
        payload.cluster = spNodeState->GetCluster();
        payload.designation = NodeUtils::GetDesignation(spNodeState->GetOperation());
    }

    if (auto const spNetworkState = wpNetworkState.lock(); spNetworkState) { 
        payload.update_timestamp = spNetworkState->GetUpdatedTimepoint().time_since_epoch().count();
    }

    if (auto const spNetworkManager = wpNetworkManager.lock(); spNetworkManager) { 
        std::ranges::transform(
            spNetworkManager->GetEndpointProtocols(),
            std::back_inserter(payload.protocols),
            [] (auto const& protocol) { return Network::ProtocolToString(protocol); });
    }

    if (auto const spPeerCache = wpPeerCache.lock(); spPeerCache) { 
        payload.neighbor_count = static_cast<std::uint32_t>(spPeerCache->ActiveCount());
    }

    return li::json_encode(payload);
}

//----------------------------------------------------------------------------------------------------------------------
