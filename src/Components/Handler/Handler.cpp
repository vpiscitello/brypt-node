//----------------------------------------------------------------------------------------------------------------------
// File: Handler.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Connect.hpp"
#include "Election.hpp"
#include "Information.hpp"
#include "Query.hpp"
#include "BryptIdentifier/ReservedIdentifiers.hpp"
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/BryptNode.hpp"
#include "BryptNode/NodeState.hpp"
#include "BryptNode/NetworkState.hpp"
#include "Components/Await/AwaitDefinitions.hpp"
#include "Components/Await/TrackingManager.hpp"
#include "Components/Peer/Proxy.hpp"
#include "Components/Peer/Manager.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Handler::IHandler> Handler::Factory(Handler::Type type, BryptNode& instance)
{
    switch (type) {
        case Handler::Type::Connect: return std::make_unique<Connect>(instance);
        case Handler::Type::Election: return std::make_unique<Election>(instance);
        case Handler::Type::Information: return std::make_unique<Information>(instance);
        case Handler::Type::Query: return std::make_unique<Query>(instance);
        case Handler::Type::Invalid:
        default: return {};
    }
}

//----------------------------------------------------------------------------------------------------------------------
// IHandler implementation
//----------------------------------------------------------------------------------------------------------------------

Handler::IHandler::IHandler(Handler::Type type, BryptNode& instance)
    : m_type(type)
    , m_instance(instance)
    , m_logger(spdlog::get(LogUtils::Name::Core.data()))
{
}

//----------------------------------------------------------------------------------------------------------------------

Handler::Type Handler::IHandler::GetType() const
{
    return m_type;
}

//----------------------------------------------------------------------------------------------------------------------

void Handler::IHandler::SendClusterNotice(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& request,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData)
{
    SendNotice(
        wpPeerProxy, request, Message::Destination::Cluster,
        noticeData, noticePhase, responsePhase, optResponseData);
}

//----------------------------------------------------------------------------------------------------------------------

void Handler::IHandler::SendNetworkNotice(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& request,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData)
{
    SendNotice(
        wpPeerProxy, request, Message::Destination::Network,
        noticeData, noticePhase, responsePhase, optResponseData);
}

//----------------------------------------------------------------------------------------------------------------------

void Handler::IHandler::SendResponse(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& request,
    std::string_view responseData,
    std::uint8_t responsePhase)
{
    // Get the current Node ID and Cluster ID for this node
    Node::SharedIdentifier spNodeIdentifier;
    auto const wpNodeState = m_instance.GetNodeState();
    if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
        spNodeIdentifier = spNodeState->GetNodeIdentifier();
    }
    assert(spNodeIdentifier);

    // Since we are responding to the request, the destination will point to its source.
    Node::Identifier destination = request.GetSourceIdentifier();

    std::optional<Message::BoundTrackerKey> optBoundAwaitTracker = {};
    std::optional<Await::TrackerKey> const optAwaitingKey = request.GetAwaitTrackerKey();
    if (optAwaitingKey) {
        optBoundAwaitTracker = { Message::AwaitBinding::Destination, *optAwaitingKey };
    }

    // Using the information from the node instance generate a discovery response message
    auto const optResponse = ApplicationMessage::Builder()
        .SetMessageContext(request.GetContext())
        .SetSource(*spNodeIdentifier)
        .SetDestination(destination)
        .SetCommand(request.GetCommand(), responsePhase)
        .SetPayload(responseData)
        .BindAwaitTracker(optBoundAwaitTracker)
        .ValidatedBuild();
    assert(optResponse);

    if (auto const spPeerProxy = wpPeerProxy.lock(); spPeerProxy) {
        [[maybe_unused]] bool const success = spPeerProxy->ScheduleSend(
            request.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
        assert(success);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Handler::IHandler::SendNotice(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& request,
    Message::Destination destination,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData = {})
{
    std::set<Node::SharedIdentifier> peers;

    // Get the information pertaining to the node itself
    Node::SharedIdentifier spNodeIdentifier;
    if (auto const spNodeState = m_instance.GetNodeState().lock()) {
        spNodeIdentifier = spNodeState->GetNodeIdentifier();
    }
    peers.emplace(spNodeIdentifier);

    // Get the information pertaining to the node's network
    if (auto const spPeerManager = m_instance.GetPeerManager().lock(); spPeerManager) {
        spPeerManager->ForEachCachedIdentifier(
            [&peers] (
                Node::SharedIdentifier const& spNodeIdentifier) -> CallbackIteration
            {
                peers.emplace(spNodeIdentifier);
                return CallbackIteration::Continue;
            }
        );
    }
    
    // Setup the awaiting message object and push this node's response
    Await::TrackerKey awaitTrackingKey = 0;
    if (auto const spAwaitManager = m_instance.GetAwaitManager().lock()) {
        awaitTrackingKey = spAwaitManager->PushRequest(wpPeerProxy, request, peers);

        if (optResponseData) {
            // Create a reading message
            auto const optNodeResponse = ApplicationMessage::Builder()
                .SetMessageContext(request.GetContext())
                .SetSource(*spNodeIdentifier)
                .SetDestination(request.GetSourceIdentifier())
                .SetCommand(request.GetCommand(), responsePhase)
                .SetPayload(*optResponseData)
                .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
                .ValidatedBuild();
            assert(optNodeResponse);
            spAwaitManager->PushResponse(*optNodeResponse);
        }
    }

    // Create a notice message for the network
    auto builder = ApplicationMessage::Builder()
        .SetMessageContext(request.GetContext())
        .SetSource(*spNodeIdentifier)
        .SetCommand(request.GetCommand(), noticePhase)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .SetPayload(noticeData);

    switch (destination) {
        case Message::Destination::Cluster: {
            builder.MakeClusterMessage();
        } break;
        case Message::Destination::Network: {
            builder.MakeNetworkMessage();
        } break;
        // Callers should not call this method for other message destination types
        default: assert(false); break;
    }

    auto const optNotice = builder.ValidatedBuild();
    assert(optNotice);

    // TODO: Handle sending notices
}

//----------------------------------------------------------------------------------------------------------------------
