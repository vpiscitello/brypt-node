//------------------------------------------------------------------------------------------------
// File: Handler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------
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
#include "Components/BryptPeer/BryptPeer.hpp"
#include "Components/BryptPeer/PeerManager.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

std::unique_ptr<Handler::IHandler> Handler::Factory(
    Handler::Type handlerType, BryptNode& instance)
{
    switch (handlerType) {
        case Handler::Type::Connect:
            return std::make_unique<Connect>(instance);

        case Handler::Type::Election:
            return std::make_unique<Election>(instance);

        case Handler::Type::Information:
            return std::make_unique<Information>(instance);

        case Handler::Type::Query:
            return std::make_unique<Query>(instance);

        case Handler::Type::Invalid:
        default:
            return {};
    }
}

//------------------------------------------------------------------------------------------------
// IHandler implementation
//------------------------------------------------------------------------------------------------

Handler::IHandler::IHandler(
    Handler::Type type, BryptNode& instance)
    : m_type(type)
    , m_instance(instance)
{
}

//------------------------------------------------------------------------------------------------

Handler::Type Handler::IHandler::GetType() const
{
    return m_type;
}

//------------------------------------------------------------------------------------------------

void Handler::IHandler::SendClusterNotice(
    std::weak_ptr<BryptPeer> const& wpBryptPeer,
    ApplicationMessage const& request,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData)
{
    SendNotice(
        wpBryptPeer, request, Message::Destination::Cluster,
        noticeData, noticePhase, responsePhase, optResponseData);
}

//------------------------------------------------------------------------------------------------

void Handler::IHandler::SendNetworkNotice(
    std::weak_ptr<BryptPeer> const& wpBryptPeer,
    ApplicationMessage const& request,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData)
{
    SendNotice(
        wpBryptPeer, request, Message::Destination::Network,
        noticeData, noticePhase, responsePhase, optResponseData);
}

//------------------------------------------------------------------------------------------------

void Handler::IHandler::SendResponse(
    std::weak_ptr<BryptPeer> const& wpBryptPeer,
    ApplicationMessage const& request,
    std::string_view responseData,
    std::uint8_t responsePhase)
{
    // Get the current Node ID and Cluster ID for this node
    BryptIdentifier::SharedContainer spBryptIdentifier;
    auto const wpNodeState = m_instance.GetNodeState();
    if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
        spBryptIdentifier = spNodeState->GetBryptIdentifier();
    }
    assert(spBryptIdentifier);

    // Since we are responding to the request, the destination will point to its source.
    BryptIdentifier::Container destination = request.GetSourceIdentifier();

    std::optional<Message::BoundTrackerKey> optBoundAwaitTracker = {};
    std::optional<Await::TrackerKey> const optAwaitingKey = request.GetAwaitTrackerKey();
    if (optAwaitingKey) {
        optBoundAwaitTracker = { Message::AwaitBinding::Destination, *optAwaitingKey };
    }

    // Using the information from the node instance generate a discovery response message
    auto const optResponse = ApplicationMessage::Builder()
        .SetMessageContext(request.GetContext())
        .SetSource(*spBryptIdentifier)
        .SetDestination(destination)
        .SetCommand(request.GetCommand(), responsePhase)
        .SetPayload(responseData)
        .BindAwaitTracker(optBoundAwaitTracker)
        .ValidatedBuild();
    assert(optResponse);

    if (auto const spBryptPeer = wpBryptPeer.lock(); spBryptPeer) {
        [[maybe_unused]] bool const success = spBryptPeer->ScheduleSend(
            request.GetContext().GetEndpointIdentifier(), optResponse->GetPack());
        assert(success);
    }
}

//------------------------------------------------------------------------------------------------

void Handler::IHandler::SendNotice(
    std::weak_ptr<BryptPeer> const& wpBryptPeer,
    ApplicationMessage const& request,
    Message::Destination destination,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData = {})
{
    std::set<BryptIdentifier::SharedContainer> peers;

    // Get the information pertaining to the node itself
    BryptIdentifier::SharedContainer spBryptIdentifier;
    if (auto const spNodeState = m_instance.GetNodeState().lock()) {
        spBryptIdentifier = spNodeState->GetBryptIdentifier();
    }
    peers.emplace(spBryptIdentifier);

    // Get the information pertaining to the node's network
    if (auto const spPeerManager = m_instance.GetPeerManager().lock(); spPeerManager) {
        spPeerManager->ForEachCachedIdentifier(
            [&peers] (
                BryptIdentifier::SharedContainer const& spBryptIdentifier) -> CallbackIteration
            {
                peers.emplace(spBryptIdentifier);
                return CallbackIteration::Continue;
            }
        );
    }
    
    // Setup the awaiting message object and push this node's response
    Await::TrackerKey awaitTrackingKey = 0;
    if (auto const spAwaitManager = m_instance.GetAwaitManager().lock()) {
        awaitTrackingKey = spAwaitManager->PushRequest(wpBryptPeer, request, peers);

        if (optResponseData) {
            // Create a reading message
            auto const optNodeResponse = ApplicationMessage::Builder()
                .SetMessageContext(request.GetContext())
                .SetSource(*spBryptIdentifier)
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
        .SetSource(*spBryptIdentifier)
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

//------------------------------------------------------------------------------------------------
