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
#include "Components/Peer/ProxyStore.hpp"
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

bool ProcessEntrypoints(
    std::shared_ptr<Peer::Proxy> const& spProxy,
    Node::SharedIdentifier const& spReferencedIdentifier,
    Network::Protocol protocolUsed,
    boost::json::array const& entrypoints,
    std::weak_ptr<BootstrapService> const& wpBootstrapService,
    std::weak_ptr<Network::Manager> const& wpNetworkManager);

bool EchoDiscoveryMessage(
    Node::SharedIdentifier const& spPeerIdentifier,
    std::span<std::uint8_t const> payload,
    std::shared_ptr<NodeState> const& spNodeState,
    std::shared_ptr<Peer::ProxyStore> const& spProxyStore);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace symbols {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Cluster = "cluster";
constexpr std::string_view Entry = "entry";
constexpr std::string_view Entrypoints = "entrypoints";
constexpr std::string_view Error = "error";
constexpr std::string_view Identifier = "identifier";
constexpr std::string_view Protocol = "protocol";

//----------------------------------------------------------------------------------------------------------------------
} // symbols namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//----------------------------------------------------------------------------------------------------------------------

// DiscoveryMessage {
//     "identifier": String,
//     "entrypoints": [
//       {
//           "protocol": String
//           "entry": String,
//       },
//       ...
//     ],
// } 

// DiscoveryResponse {
//     "identifier": String (dependent),
//     "entrypoints": [
//       {
//           "protocol": String
//           "entry": String,
//       },
//       ...
//     ] (dependent),
//     "error": String (optional)
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
    
    m_wpProxyStore = spServiceProvider->Fetch<Peer::ProxyStore>();
    if (m_wpProxyStore.expired()) { return false; }

    return GenerateResponsePayload();
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryHandler::OnMessage(
    Message::Application::Parcel const& message, Peer::Action::Next& next)
{
    auto const spProxy = next.GetProxy().lock();
    if (!spProxy) { return false; }

    auto const spNodeState = m_wpNodeState.lock();
    if (!spNodeState) { return false; }

    auto const spProxyStore = m_wpProxyStore.lock();
    if (!spProxyStore) { return false; }
    
    auto request = boost::json::parse(message.GetPayload().GetStringView()).as_object();
    auto const& identifier = request.at(symbols::Identifier).as_string();
    auto const& entrypoints = request.at(symbols::Entrypoints).as_array();

    auto const spPeerIdentifier = spProxy->GetIdentifier();
    auto const spReferencedIdentifier = std::make_shared<Node::Identifier const>(identifier.c_str());

    // The discovery request must contain a valid identifier and the entrypoints the peer is hosting. 
    if (!spReferencedIdentifier->IsValid() || entrypoints.empty()) { return false; }

    // If the message is being echoed for a peer and that peer is already active, there is nothing else 
    // we need to do. 
    if (*spPeerIdentifier != *spReferencedIdentifier && spProxyStore->IsActive(*spReferencedIdentifier)) {
        return true;
    }

    // Process the provided entrypoints. This will cause this node to connect to the peer's hosted endpoints. 
    bool const processed = local::ProcessEntrypoints(
        spProxy,
        spReferencedIdentifier,
        message.GetContext().GetEndpointProtocol(),
        entrypoints,
        m_wpBootstrapService,
        m_wpNetworkManager);

    if (!processed) { return false; }
    
    // Distribute the new peer's discovery message to the known neighbors, such that they can attempt a 
    // connection with the joiner. 
    if (auto const optEcho = message.GetExtension<Message::Extension::Echo>(); !optEcho) {
        local::EchoDiscoveryMessage(
            spPeerIdentifier, message.GetPayload().GetReadableView(), spNodeState, spProxyStore);
    }

    // If the original message is a request, create and send off a response. 
    if (auto const optAwaitable = message.GetExtension<Message::Extension::Awaitable>(); optAwaitable) {
        return next.Respond(m_response, Message::Extension::Status::Accepted);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryHandler::GenerateResponsePayload()
{
    auto const spNodeState = m_wpNodeState.lock();
    auto const spNetworkManager = m_wpNetworkManager.lock();

    if (!spNodeState || !spNetworkManager) { return false; }

    boost::json::object json;

    json[symbols::Identifier] = spNodeState->GetNodeIdentifier()->ToExternal();

    boost::json::array entrypoints;
    spNetworkManager->ForEach([&entrypoints] (Network::Endpoint::Identifier, Network::BindingAddress const& binding) {
        boost::json::object entrypoint;
        entrypoint[symbols::Protocol] = Network::ProtocolToString(binding.GetProtocol());
        entrypoint[symbols::Entry] = binding.GetUri();
        entrypoints.emplace_back(std::move(entrypoint));
        return CallbackIteration::Continue;
    });

    json[symbols::Entrypoints] = std::move(entrypoints);

    m_response = Message::Payload{ std::make_shared<std::string const>(boost::json::serialize(json)) };

    return !m_response.IsEmpty();
}

//----------------------------------------------------------------------------------------------------------------------

Route::Fundamental::Connect::DiscoveryProtocol::DiscoveryProtocol()
    : m_wpNodeState()
    , m_wpBootstrapService()
    , m_wpNetworkManager()
    , m_wpProxyStore()
    , m_payload()
    , m_logger(spdlog::get(Logger::Name::Core.data()))
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryProtocol::CompileRequest(
    std::shared_ptr<Node::ServiceProvider> const& spServiceProvider)
{
    m_wpNodeState = spServiceProvider->Fetch<NodeState>();
    if (m_wpNodeState.expired()) { return false; }

    m_wpBootstrapService = spServiceProvider->Fetch<BootstrapService>();
    if (m_wpBootstrapService.expired()) { return false; }

    m_wpNetworkManager = spServiceProvider->Fetch<Network::Manager>();
    if (m_wpNetworkManager.expired()) { return false; }

    m_wpProxyStore = spServiceProvider->Fetch<Peer::ProxyStore>();
    if (m_wpProxyStore.expired()) { return false; }

    return GenerateRequestPayload();
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryProtocol::SendRequest(
    std::shared_ptr<Peer::Proxy> const& spProxy, Message::Context const& context) const
{
    assert(!m_payload.IsEmpty());

    auto const spNodeState = m_wpNodeState.lock();
    if (!spNodeState) { return false; }

    auto builder = Message::Application::Parcel::GetBuilder()
        .SetContext(context)
        .SetSource(*spNodeState->GetNodeIdentifier())
        .SetRoute(Route::Fundamental::Connect::DiscoveryHandler::Path)
        .SetPayload(m_payload);

    auto const optTrackerKey = spProxy->Request(builder,
        [this, wpProxy = std::weak_ptr<Peer::Proxy>{ spProxy }] (auto const& response) {
            auto const spProxy = wpProxy.lock();
            if (!spProxy || response.GetPayload().IsEmpty()) { return; }

            auto const spNodeState = m_wpNodeState.lock();
            if (!spNodeState) { return; }

            auto const spProxyStore = m_wpProxyStore.lock();
            if (!spProxyStore) { return; }

            auto const json = boost::json::parse(response.GetPayload().GetStringView()).as_object();
            if (auto const itr = json.find(symbols::Error); itr != json.end()) {
                m_logger->warn("The peer ({}) failed to accept the discovery request", response.GetSource());
                return;
            }

            auto const& entrypoints = json.at(symbols::Entrypoints).as_array();
            if (entrypoints.empty()) { return; }

            // Process any additional entrypoints the peer sent back. We may have only known of some of them 
            // when we first connected. 
            local::ProcessEntrypoints(
                spProxy,
                spProxy->GetIdentifier(),
                response.GetEndpointProtocol(),
                entrypoints,
                m_wpBootstrapService,
                m_wpNetworkManager);
            
            // Echo the peer's entrypoints, there's a chance we are merging networks. 
            local::EchoDiscoveryMessage(
                spProxy->GetIdentifier(), response.GetPayload().GetReadableView(), spNodeState, spProxyStore);
        },
        [this] (auto const& response) {
            m_logger->warn("Encountered an error waiting for discovery response from: {}", response.GetSource());
        });
    
    return optTrackerKey.has_value();
}

//----------------------------------------------------------------------------------------------------------------------

bool Route::Fundamental::Connect::DiscoveryProtocol::GenerateRequestPayload()
{
    auto const spNodeState = m_wpNodeState.lock();
    auto const spNetworkManager = m_wpNetworkManager.lock();

    if (!spNodeState || !spNetworkManager) { return false; }

    boost::json::object json;

    json[symbols::Identifier] = spNodeState->GetNodeIdentifier()->ToExternal();

    boost::json::array entrypoints;
    spNetworkManager->ForEach([&entrypoints](Network::Endpoint::Identifier, Network::BindingAddress const& binding) {
        boost::json::object entrypoint;
        entrypoint[symbols::Protocol] = Network::ProtocolToString(binding.GetProtocol());
        entrypoint[symbols::Entry] = binding.GetUri();
        entrypoints.emplace_back(std::move(entrypoint));
        return CallbackIteration::Continue;
    });
    json[symbols::Entrypoints] = std::move(entrypoints);

    m_payload = Message::Payload{ std::make_shared<std::string const>(boost::json::serialize(json)) };

    return !m_payload.IsEmpty();
}

//----------------------------------------------------------------------------------------------------------------------

bool local::ProcessEntrypoints(
    std::shared_ptr<Peer::Proxy> const& spProxy,
    Node::SharedIdentifier const& spReferencedIdentifier,
    Network::Protocol protocolUsed,
    boost::json::array const& entrypoints,
    std::weak_ptr<BootstrapService> const& wpBootstrapService,
    std::weak_ptr<Network::Manager> const& wpNetworkManager)
{
    auto const spBootstrapService = wpBootstrapService.lock();
    auto const spNetworkManager = wpNetworkManager.lock();

    // If the identifier provided in the message matches the proxy's identifier, then the provided entrypoints
    // should be associated with that peer. Otherwise, the entrypoints have been echoed and should not 
    // be associated with the echoer. 
    assert(spProxy->GetIdentifier() && spReferencedIdentifier);
    bool const echoed = *spProxy->GetIdentifier() != *spReferencedIdentifier;

    // For each listed entrypoint, handle each entry for the given protocol.
    for (auto const& item : entrypoints) {
        auto const& entrypoint = item.as_object();
        auto const& protocolField = entrypoint.at(symbols::Protocol).as_string();
        auto const& entryField = entrypoint.at(symbols::Entry).as_string();

        // Parse the technology type from the human readable name.
        auto const protocol = Network::ParseProtocol(protocolField.c_str());
        Network::RemoteAddress address(protocol, entryField.c_str(), true);
        if (!address.IsValid()) {
            continue; // TODO: Should something happen if an entrypoint is not valid? 
        }

        // If the message was not echoed on behalf of the new peer, then it can be associated with the current
        // proxy. When we connect to the new peer directly we will be able to associate addresses. 
        if (!echoed) { spProxy->AssociateRemote(address); }

        // Store the entrypoint in the bootstrap sevice, if this node needs to establish a connection first
        // it will be able to fetch the address. 
        if (spBootstrapService) [[likely]] { spBootstrapService->InsertBootstrap(address); }
          
        // We should only make a new connection if the message was echoed, or this is a new protocol used. 
        if (bool const connectable = echoed || protocol != protocolUsed; connectable) {
            // If there is an entrypoint for a different protocol other than the one we are currently using, 
            // then schedule a connection. Otherwise, there is no need to establish a second connection. 
            if (spNetworkManager) [[likely]] {
                if (auto const spEndpoint = spNetworkManager->GetEndpoint(protocol); spEndpoint) {
                    [[maybe_unused]] bool const success = spEndpoint->ScheduleConnect(
                        std::move(address), spReferencedIdentifier);
                }
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::EchoDiscoveryMessage(
    Node::SharedIdentifier const & spPeerIdentifier,
    std::span<std::uint8_t const> payload,
    std::shared_ptr<NodeState> const& spNodeState,
    std::shared_ptr<Peer::ProxyStore> const & spProxyStore)
{
    spProxyStore->ForEach([&](std::shared_ptr<Peer::Proxy> const& spNeighbor) {
        // Echo the message to every neighbor except for the current peer. 
        if (spNeighbor->GetIdentifier() != spPeerIdentifier) {
            auto builder = Message::Application::Parcel::GetBuilder()
                .SetSource(*spNodeState->GetNodeIdentifier())
                .SetRoute(Route::Fundamental::Connect::DiscoveryHandler::Path)
                .SetPayload(Message::Payload{ payload })
                .BindExtension<Message::Extension::Echo>();

            [[maybe_unused]] bool const success = spNeighbor->ScheduleSend(builder);
        }

        return CallbackIteration::Continue;
    });

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
