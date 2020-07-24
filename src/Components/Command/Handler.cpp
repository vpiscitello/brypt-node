//------------------------------------------------------------------------------------------------
// File: ConnectHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------
#include "ConnectHandler.hpp"
#include "ElectionHandler.hpp"
#include "InformationHandler.hpp"
#include "QueryHandler.hpp"
#include "TransformHandler.hpp"
#include "../Await/Await.hpp"
#include "../MessageQueue/MessageQueue.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../BryptNode/NodeState.hpp"
#include "../../BryptNode/NetworkState.hpp"
#include "../../Message/Message.hpp"
#include "../../Message/MessageBuilder.hpp"
#include "../../Utilities/ReservedIdentifiers.hpp"
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
    CMessage const& request,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData)
{
    SendNotice(
        request,
        static_cast<NodeUtils::NodeIdType>(ReservedIdentifiers::ClusterRequest),
        noticeData,
        noticePhase,
        responsePhase,
        optResponseData);
}

//------------------------------------------------------------------------------------------------

void Command::IHandler::SendNetworkNotice(
    CMessage const& request,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData)
{
    SendNotice(
        request,
        static_cast<NodeUtils::NodeIdType>(ReservedIdentifiers::NetworkRequest),
        noticeData,
        noticePhase,
        responsePhase,
        optResponseData);
}

//------------------------------------------------------------------------------------------------

void Command::IHandler::SendResponse(
    CMessage const& request,
    std::string_view responseData,
    std::uint8_t responsePhase,
    std::optional<NodeUtils::NodeIdType> optDestinationOverride)
{
    // Get the current Node ID and Cluster ID for this node
    NodeUtils::NodeIdType id = static_cast<NodeUtils::NodeIdType>(ReservedIdentifiers::Invalid);
    auto const wpNodeState = m_instance.GetNodeState();
    if (auto const spNodeState = wpNodeState.lock(); spNodeState) {
        id = spNodeState->GetId();
    }

    NodeUtils::NodeIdType destination = request.GetSource();
    if (optDestinationOverride) {
        destination = *optDestinationOverride;
    }

    std::optional<Message::BoundAwaitingKey> optBoundAwaitId = {};
    std::optional<NodeUtils::ObjectIdType> const optAwaitingKey = request.GetAwaitingKey();
    if (optAwaitingKey) {
        optBoundAwaitId = { Message::AwaitBinding::Destination, *optAwaitingKey };
    }

    // Using the information from the node instance generate a discovery response message
    OptionalMessage const optResponse = CMessage::Builder()
        .SetMessageContext(request.GetMessageContext())
        .SetSource(id)
        .SetDestination(destination)
        .SetCommand(request.GetCommandType(), responsePhase)
        .SetData(responseData,  request.GetNonce() + 1)
        .ValidatedBuild();
    assert(optResponse);

    // Get the message queue from the node instance and forward the response
    auto const wpMessageQueue = m_instance.GetMessageQueue();
    if (auto const spMessageQueue = wpMessageQueue.lock(); spMessageQueue) {
        spMessageQueue->PushOutgoingMessage(*optResponse);
    }
}

//------------------------------------------------------------------------------------------------

void Command::IHandler::SendNotice(
    CMessage const& request,
    NodeUtils::NodeIdType noticeDestination,
    std::string_view noticeData,
    std::uint8_t noticePhase,
    std::uint8_t responsePhase,
    std::optional<std::string> optResponseData = {})
{
    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = 0;
    if (auto const spNodeState = m_instance.GetNodeState().lock()) {
        id = spNodeState->GetId();
    }

    // Get the information pertaining to the node's network
    std::set<NodeUtils::NodeIdType> peers;
    if (auto const spNetworkState = m_instance.GetNetworkState().lock()) {
        peers = spNetworkState->GetPeerNames();
    }
    
    // Setup the awaiting message object and push this node's response
    NodeUtils::ObjectIdType awaitObjectKey = 0;
    if (auto const awaiting = m_instance.GetAwaiting().lock()) {
        awaitObjectKey = awaiting->PushRequest(request, peers);

        if (optResponseData) {
            // Create a reading message
            OptionalMessage const optNodeResponse = CMessage::Builder()
                .SetMessageContext(request.GetMessageContext())
                .SetSource(id)
                .SetDestination(request.GetSource())
                .SetCommand(request.GetCommandType(), responsePhase)
                .SetData(*optResponseData, request.GetNonce() + 1)
                .BindAwaitingKey(Message::AwaitBinding::Destination, awaitObjectKey)
                .ValidatedBuild();
            assert(optNodeResponse);
            awaiting->PushResponse(*optNodeResponse);
        }
    }

    // Create a notice message for the network
    OptionalMessage const optNotice = CMessage::Builder()
        .SetMessageContext(request.GetMessageContext())
        .SetSource(id)
        .SetDestination(noticeDestination)
        .SetCommand(request.GetCommandType(), noticePhase)
        .SetData(noticeData, request.GetNonce() + 1)
        .BindAwaitingKey(Message::AwaitBinding::Source, awaitObjectKey)
        .ValidatedBuild();
    assert(optNotice);

    // TODO: Handle sending notices
}

//------------------------------------------------------------------------------------------------
