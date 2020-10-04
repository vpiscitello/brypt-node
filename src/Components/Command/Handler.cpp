//------------------------------------------------------------------------------------------------
// File: Handler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------
#include "ConnectHandler.hpp"
#include "ElectionHandler.hpp"
#include "InformationHandler.hpp"
#include "QueryHandler.hpp"
#include "../Await/AwaitDefinitions.hpp"
#include "../Await/TrackingManager.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../BryptPeer/PeerManager.hpp"
#include "../../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../BryptNode/NodeState.hpp"
#include "../../BryptNode/NetworkState.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

std::unique_ptr<Command::IHandler> Command::Factory(
    Command::Type commandType,
    CBryptNode& instance)
{
    switch (commandType) {
        case Command::Type::Connect:
            return std::make_unique<CConnectHandler>(instance);

        case Command::Type::Election:
            return std::make_unique<CElectionHandler>(instance);

        case Command::Type::Information:
            return std::make_unique<CInformationHandler>(instance);

        case Command::Type::Query:
            return std::make_unique<CQueryHandler>(instance);

        case Command::Type::Invalid:
        default:
            return {};
    }
}

//------------------------------------------------------------------------------------------------
// IHandler implementation
//------------------------------------------------------------------------------------------------

Command::IHandler::IHandler(
    Command::Type type,
    CBryptNode& instance)
    : m_type(type)
    , m_instance(instance)
{
}

//------------------------------------------------------------------------------------------------

Command::Type Command::IHandler::GetType() const
{
    return m_type;
}

//------------------------------------------------------------------------------------------------

void Command::IHandler::SendClusterNotice(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CApplicationMessage const& request,
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

void Command::IHandler::SendNetworkNotice(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CApplicationMessage const& request,
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

void Command::IHandler::SendResponse(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CApplicationMessage const& request,
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
    BryptIdentifier::CContainer destination = request.GetSourceIdentifier();

    std::optional<Message::BoundTrackerKey> optBoundAwaitTracker = {};
    std::optional<Await::TrackerKey> const optAwaitingKey = request.GetAwaitTrackerKey();
    if (optAwaitingKey) {
        optBoundAwaitTracker = { Message::AwaitBinding::Destination, *optAwaitingKey };
    }

    // Using the information from the node instance generate a discovery response message
    auto const optResponse = CApplicationMessage::Builder()
        .SetMessageContext(request.GetMessageContext())
        .SetSource(*spBryptIdentifier)
        .SetDestination(destination)
        .SetCommand(request.GetCommand(), responsePhase)
        .SetData(responseData)
        .BindAwaitTracker(optBoundAwaitTracker)
        .ValidatedBuild();
    assert(optResponse);

    if (auto const spBryptPeer = wpBryptPeer.lock(); spBryptPeer) {
        spBryptPeer->ScheduleSend(*optResponse);
    }
}

//------------------------------------------------------------------------------------------------

void Command::IHandler::SendNotice(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CApplicationMessage const& request,
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
            auto const optNodeResponse = CApplicationMessage::Builder()
                .SetMessageContext(request.GetMessageContext())
                .SetSource(*spBryptIdentifier)
                .SetDestination(request.GetSourceIdentifier())
                .SetCommand(request.GetCommand(), responsePhase)
                .SetData(*optResponseData)
                .BindAwaitTracker(Message::AwaitBinding::Destination, awaitTrackingKey)
                .ValidatedBuild();
            assert(optNodeResponse);
            spAwaitManager->PushResponse(*optNodeResponse);
        }
    }

    // Create a notice message for the network
    auto builder = CApplicationMessage::Builder()
        .SetMessageContext(request.GetMessageContext())
        .SetSource(*spBryptIdentifier)
        .SetCommand(request.GetCommand(), noticePhase)
        .BindAwaitTracker(Message::AwaitBinding::Source, awaitTrackingKey)
        .SetData(noticeData);

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
