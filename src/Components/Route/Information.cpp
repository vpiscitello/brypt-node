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
#include <boost/json.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace symbols {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Cluster = "cluster";
constexpr std::string_view NeighborCount = "neighbor_count";
constexpr std::string_view Designation = "designation";
constexpr std::string_view Protocols = "protocols";
constexpr std::string_view UpdateTimestamp = "update_timestamp";

std::string GenerateNodeInfo(
    std::weak_ptr<NodeState> const& wpNodeState,
    std::weak_ptr<NetworkState> const& wpNetworkState,
    std::weak_ptr<Network::Manager> const& wpNetworkManager,
    std::weak_ptr<IPeerCache> const& wpPeerCache);

//----------------------------------------------------------------------------------------------------------------------
} // symbols namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//----------------------------------------------------------------------------------------------------------------------
//
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
        symbols::GenerateNodeInfo(m_wpNodeState, m_wpNetworkState, m_wpNetworkManager, m_wpPeerCache),
        Message::Extension::Status::Ok);
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
            .payload = symbols::GenerateNodeInfo(m_wpNodeState, m_wpNetworkState, m_wpNetworkManager, m_wpPeerCache)
        }
    });

    return optTrackerKey.has_value();
}

//----------------------------------------------------------------------------------------------------------------------

std::string symbols::GenerateNodeInfo(
    std::weak_ptr<NodeState> const& wpNodeState,
    std::weak_ptr<NetworkState> const& wpNetworkState,
    std::weak_ptr<Network::Manager> const& wpNetworkManager,
    std::weak_ptr<IPeerCache> const& wpPeerCache)
{
    boost::json::object json;
    if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
        json[symbols::Cluster] = spNodeState->GetCluster();
        json[symbols::Designation] = NodeUtils::GetDesignation(spNodeState->GetOperation());
    }

    if (auto const spNetworkState = wpNetworkState.lock(); spNetworkState) { 
        json[symbols::UpdateTimestamp] = spNetworkState->GetUpdatedTimepoint().time_since_epoch().count();
    }
    
    boost::json::array protocols;
    if (auto const spNetworkManager = wpNetworkManager.lock(); spNetworkManager) { 
        for (auto const& protocol : spNetworkManager->GetEndpointProtocols()) {
            protocols.emplace_back(Network::ProtocolToString(protocol));
        }
    }

    json[symbols::Protocols] = std::move(protocols);

    if (auto const spPeerCache = wpPeerCache.lock(); spPeerCache) {
        json[symbols::NeighborCount] = spPeerCache->ActiveCount();
    }

    return boost::json::serialize(json);
}

//----------------------------------------------------------------------------------------------------------------------
