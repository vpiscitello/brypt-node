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
#include "TransformHandler.hpp"
#include "../Await/AwaitDefinitions.hpp"
#include "../Await/TrackingManager.hpp"
#include "../BryptPeer/BryptPeer.hpp"
#include "../MessageControl/MessageCollector.hpp"
#include "../../BryptIdentifier/ReservedIdentifiers.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../BryptNode/NodeState.hpp"
#include "../../BryptNode/NetworkState.hpp"
#include "../../Configuration/PeerPersistor.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
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

        case Command::Type::Transform:
            return std::make_unique<CTransformHandler>(instance);

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
    CMessage const& request,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData)
{
    static BryptIdentifier::CContainer const identifier(ReservedIdentifiers::Network::ClusterRequest);
    SendNotice(wpBryptPeer, request, identifier, noticeData, noticePhase, responsePhase, optResponseData);
}

//------------------------------------------------------------------------------------------------

void Command::IHandler::SendNetworkNotice(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CMessage const& request,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData)
{
    static BryptIdentifier::CContainer const identifier(ReservedIdentifiers::Network::NetworkRequest);
    SendNotice(wpBryptPeer, request, identifier, noticeData, noticePhase, responsePhase, optResponseData);
}

//------------------------------------------------------------------------------------------------

void Command::IHandler::SendResponse(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CMessage const& request,
    std::string_view responseData,
    std::uint8_t responsePhase,
    std::optional<BryptIdentifier::CContainer> optDestinationOverride)
{
    // Get the current Node ID and Cluster ID for this node
    BryptIdentifier::SharedContainer spBryptIdentifier;
    auto const wpNodeState = m_instance.GetNodeState();
    if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
        spBryptIdentifier = spNodeState->GetBryptIdentifier();
    }
    assert(spBryptIdentifier);

    BryptIdentifier::CContainer destination = request.GetSource();
    if (optDestinationOverride) {
        destination = *optDestinationOverride;
    }

    std::optional<Message::BoundAwaitingKey> optBoundAwaitId = {};
    std::optional<Await::TrackerKey> const optAwaitingKey = request.GetAwaitingKey();
    if (optAwaitingKey) {
        optBoundAwaitId = { Message::AwaitBinding::Destination, *optAwaitingKey };
    }

    // Using the information from the node instance generate a discovery response message
    OptionalMessage const optResponse = CMessage::Builder()
        .SetMessageContext(request.GetMessageContext())
        .SetSource(*spBryptIdentifier)
        .SetDestination(destination)
        .SetCommand(request.GetCommandType(), responsePhase)
        .SetData(responseData,  request.GetNonce() + 1)
        .ValidatedBuild();
    assert(optResponse);

    if (auto const spBryptPeer = wpBryptPeer.lock(); spBryptPeer) {
        spBryptPeer->ScheduleSend(*optResponse);
    }
}

//------------------------------------------------------------------------------------------------

void Command::IHandler::SendNotice(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CMessage const& request,
    BryptIdentifier::CContainer noticeDestination,
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
    if (auto const spPeerPersistor = m_instance.GetPeerPersistor().lock()) {
        spPeerPersistor->ForEachCachedIdentifier(
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
            OptionalMessage const optNodeResponse = CMessage::Builder()
                .SetMessageContext(request.GetMessageContext())
                .SetSource(*spBryptIdentifier)
                .SetDestination(request.GetSource())
                .SetCommand(request.GetCommandType(), responsePhase)
                .SetData(*optResponseData, request.GetNonce() + 1)
                .BindAwaitingKey(Message::AwaitBinding::Destination, awaitTrackingKey)
                .ValidatedBuild();
            assert(optNodeResponse);
            spAwaitManager->PushResponse(*optNodeResponse);
        }
    }

    // Create a notice message for the network
    OptionalMessage const optNotice = CMessage::Builder()
        .SetMessageContext(request.GetMessageContext())
        .SetSource(*spBryptIdentifier)
        .SetDestination(noticeDestination)
        .SetCommand(request.GetCommandType(), noticePhase)
        .SetData(noticeData, request.GetNonce() + 1)
        .BindAwaitingKey(Message::AwaitBinding::Source, awaitTrackingKey)
        .ValidatedBuild();
    assert(optNotice);

    // TODO: Handle sending notices
}

//------------------------------------------------------------------------------------------------
