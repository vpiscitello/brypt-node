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
#include "Components/Peer/Action.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/State/NodeState.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json.hpp>
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <vector>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

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
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

Route::Fundamental::Connect::DiscoveryProtocol::DiscoveryProtocol(
    Configuration::Options::Endpoints const& endpoints, std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
    : m_spNodeIdentifier()
    , m_wpNetworkManager(spServiceProvider->Fetch<Network::Manager>())
    , m_spSharedPayload(std::make_shared<std::string>(local::GenerateDiscoveryData(endpoints)))
    , m_logger(spdlog::get(Logger::Name::Core.data()))
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryProtocol::SendRequest(
    std::shared_ptr<Peer::Proxy> const& spProxy, Message::Context const& context) const
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------
