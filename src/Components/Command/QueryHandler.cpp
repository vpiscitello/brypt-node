//------------------------------------------------------------------------------------------------
// File: QueryHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "QueryHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Await/Await.hpp"
#include "../Connection/Connection.hpp"
#include "../MessageQueue/MessageQueue.hpp"
#include "../Notifier/Notifier.hpp"
#include "../BryptNode/BryptNode.hpp"
#include "../BryptNode/NodeState.hpp"
#include "../BryptNode/NetworkState.hpp"
//------------------------------------------------------------------------------------------------
#include "../../Libraries/metajson/metajson.hh"
#include <set>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------

std::string GenerateReading();

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace Json {
//------------------------------------------------------------------------------------------------

struct TReading;

//------------------------------------------------------------------------------------------------
} // Json namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//------------------------------------------------------------------------------------------------
IOD_SYMBOL(reading)
IOD_SYMBOL(timestamp)
//------------------------------------------------------------------------------------------------

struct Json::TReading
{
    TReading(
        std::uint32_t reading,
        std::string const& timestamp)
        : reading(reading)
        , timestamp(timestamp)
    {
    }
    std::uint32_t const reading;
    std::string const timestamp;
};

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Command::CQueryHandler::CQueryHandler(CBryptNode& instance)
    : IHandler(Command::Type::Query, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Information message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQueryHandler::HandleMessage(CMessage const& message)
{
    bool status = false;

    auto const phase = static_cast<CQueryHandler::Phase>(message.GetPhase());
    switch (phase) {
        case CQueryHandler::Phase::Flood: {
            status = FloodHandler(message);
        } break;
        case CQueryHandler::Phase::Respond: {
            status = RespondHandler(message);
        } break;
        case CQueryHandler::Phase::Aggregate: {
            status = AggregateHandler(message);
        } break;
        case CQueryHandler::Phase::Close: {
            status = CloseHandler();
        } break;
        default: break;
    }

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the flood phase for the Query type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQueryHandler::FloodHandler(CMessage const& message)
{
    printo("Sending notification for Query request", NodeUtils::PrintType::Command);

    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = 0;
    if (auto const spNodeState = m_instance.GetNodeState().lock()) {
        id = spNodeState->GetId();
    }

    // Get the information pertaining to the node's network
    std::set<NodeUtils::NodeIdType> peerNames;
    if (auto const spNetworkState = m_instance.GetNetworkState().lock()) {
        peerNames = spNetworkState->GetPeerNames();
    }
    
    NodeUtils::NetworkNonce const nonce = 0;

    // Setup the awaiting message object and push this node's response
    NodeUtils::ObjectIdType awaitKey = 0;
    if (auto const awaiting = m_instance.GetAwaiting().lock()) {
        awaitKey = awaiting->PushRequest(message, peerNames);

        // Create a reading message
        CMessage const readingMessage(
            id, message.GetSourceId(),
            Command::Type::Query, static_cast<std::uint8_t>(CQueryHandler::Phase::Aggregate),
            local::GenerateReading(), nonce,
            Message::BoundAwaitId(
                {Message::AwaitBinding::Destination, awaitKey}));

        awaiting->PushResponse(readingMessage);
    }

    // Create a notice message for the network
    CMessage const notice(
        id, 0xFFFFFFFF,
        Command::Type::Query, static_cast<std::uint8_t>(Phase::Respond),
        "Request for Sensor Readings.", nonce,
        Message::BoundAwaitId(
            {Message::AwaitBinding::Source, awaitKey}));

    // Send the notice via the network notifier connection
    if (auto const notifier = m_instance.GetNotifier().lock()) {
        notifier->Send(notice, NodeUtils::NotificationType::Network);
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the respond phase for the Query type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQueryHandler::RespondHandler(CMessage const& message)
{
    printo("Building response for Query request", NodeUtils::PrintType::Command);
    NodeUtils::NodeIdType id = 0;
    if (auto const spNodeState = m_instance.GetNodeState().lock()) {
        id = spNodeState->GetId();
    }

    NodeUtils::NodeIdType destinationId = message.GetSourceId();
    // If there is an await id attached to the message append it to the destinationId
    std::optional<NodeUtils::ObjectIdType> const& optAwaitId = message.GetAwaitId();

    NodeUtils::NetworkNonce const nonce = message.GetNonce() + 1;
    CMessage const request(
        id,
        destinationId,
        Command::Type::Query,
        static_cast<std::uint8_t>(Phase::Aggregate),
        local::GenerateReading(),
        nonce,
        Message::BoundAwaitId(
            {Message::AwaitBinding::Destination, *optAwaitId}));

    // TODO: Add method to defer if node instance is a coordinator
    if (auto const optConnection = m_instance.GetConnection(destinationId)) {
        auto const connection = optConnection->lock();
        connection->Send(request);
        std::optional<std::string> optResponse = connection->Receive();
        if (optResponse) {
            try {
                CMessage response(*optResponse);
                NodeUtils::printo("Received: " + message.GetPack(), NodeUtils::PrintType::Command);
            } catch (...) {
                NodeUtils::printo("Query response could not be unpacked!", NodeUtils::PrintType::Command);
            }
        }
    }

    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the aggregate phase for the Query type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQueryHandler::AggregateHandler(CMessage const& message)
{
    printo("Pushing response to AwaitObject", NodeUtils::PrintType::Command);
    if (auto const awaiting = m_instance.GetAwaiting().lock()) {
        awaiting->PushResponse(message);
    }

    // Need to track encryption keys and nonces
    NodeUtils::NodeIdType id = 0;
    if (auto const spNodeState = m_instance.GetNodeState().lock()) {
        id = spNodeState->GetId();
    }

    NodeUtils::NodeIdType const& destinationId = message.GetSourceId();
    NodeUtils::NetworkNonce const nonce = message.GetNonce() + 1;
    CMessage const response(
        id,
        destinationId,
        Command::Type::Query,
        static_cast<std::uint8_t>(Phase::Close),
        "Message Response",
        nonce);

    if (auto const messageQueue = m_instance.GetMessageQueue().lock()) {
        messageQueue->PushOutgoingMessage(destinationId, response);
    }
    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the close phase for the Query type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQueryHandler::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Generate a pseudo-random value for the temperature reading and pack it into JSON
// Returns: The JSON structure as a string
//------------------------------------------------------------------------------------------------
std::string local::GenerateReading()
{
    std::int32_t const value = rand() % ( 74 - 68 ) + 68;
    Json::TReading const reading(value, NodeUtils::GetSystemTimestamp());
    return iod::json_object(s::reading, s::timestamp).encode(reading);
}

//------------------------------------------------------------------------------------------------
