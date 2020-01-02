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
Command::CQuery::CQuery(CNode& instance, std::weak_ptr<CState> const& state)
    : CHandler(instance, state)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Command::CQuery::whatami()
{
    printo("Handling response to Query request", NodeUtils::PrintType::COMMAND);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Information message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQuery::HandleMessage(CMessage const& message)
{
    bool status = false;
    whatami();

    auto const phase = static_cast<CQuery::Phase>(message.GetPhase());
    switch (phase) {
        case CQuery::Phase::FLOOD: {
            status = FloodHandler(message);
        } break;
        case CQuery::Phase::RESPOND: {
            status = RespondHandler(message);
        } break;
        case CQuery::Phase::AGGREGATE: {
            status = AggregateHandler(message);
        } break;
        case CQuery::Phase::CLOSE: {
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
bool Command::CQuery::FloodHandler(CMessage const& message)
{
    printo("Sending notification for Query request", NodeUtils::PrintType::COMMAND);

    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = 0;
    if (auto const selfState = m_state.lock()->GetSelfState().lock()) {
        id = selfState->GetId();
    }

    // Get the information pertaining to the node's network
    std::set<NodeUtils::NodeIdType> peerNames;
    if (auto const networkState = m_state.lock()->GetNetworkState().lock()) {
        peerNames = networkState->GetPeerNames();
    }

    // Create a reading message
    NodeUtils::NetworkNonce const nonce = 0;
    CMessage readingMessage(
        id,
        message.GetSourceId(),
        NodeUtils::CommandType::QUERY,
        static_cast<std::uint32_t>(CQuery::Phase::AGGREGATE),
        local::GenerateReading(),
        nonce);
    
    // Setup the awaiting message object and push this node's response
    NodeUtils::ObjectIdType awaitKey = 0;
    if (auto const awaiting = m_instance.GetAwaiting().lock()) {
        awaitKey = awaiting->PushRequest(message, peerNames);
        awaiting->PushResponse(awaitKey, readingMessage);
    }

    // Create a notice message for the network
    CMessage const notice(
        id, 0xFFFFFFFF,
        NodeUtils::CommandType::QUERY, static_cast<std::uint32_t>(Phase::RESPOND),
        "Request for Sensor Readings.", nonce,
        Message::BoundAwaitId(
            {Message::AwaitBinding::SOURCE, awaitKey}));

    // Send the notice via the network notifier connection
    if (auto const notifier = m_instance.GetNotifier().lock()) {
        notifier->Send(notice, NodeUtils::NotificationType::NETWORK);
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the respond phase for the Query type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQuery::RespondHandler(CMessage const& message)
{
    printo("Building response for Query request", NodeUtils::PrintType::COMMAND);
    NodeUtils::NodeIdType id = 0;
    if (auto const selfState = m_state.lock()->GetSelfState().lock()) {
        id = selfState->GetId();
    }

    NodeUtils::NodeIdType destinationId = message.GetSourceId();
    // If there is an await id attached to the message append it to the destinationId
    std::optional<NodeUtils::ObjectIdType> const& optAwaitId = message.GetAwaitId();

    NodeUtils::NetworkNonce const nonce = message.GetNonce() + 1;
    CMessage const request(
        id,
        destinationId,
        NodeUtils::CommandType::QUERY,
        static_cast<std::uint32_t>(Phase::AGGREGATE),
        local::GenerateReading(),
        nonce,
        Message::BoundAwaitId(
            {Message::AwaitBinding::DESTINATION, *optAwaitId}));

    // TODO: Add method to defer if node instance is a coordinator
    if (auto const optConnection = m_instance.GetConnection(destinationId)) {
        auto const connection = optConnection->lock();
        connection->Send(request);
        std::optional<std::string> optResponse = connection->Receive();
        if (optResponse) {
            try {
                CMessage response(*optResponse);
                NodeUtils::printo("Received: " + message.GetPack(), NodeUtils::PrintType::COMMAND);
            } catch (...) {
                NodeUtils::printo("Query response could not be unpacked!", NodeUtils::PrintType::COMMAND);
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
bool Command::CQuery::AggregateHandler(CMessage const& message)
{
    printo("Pushing response to AwaitObject", NodeUtils::PrintType::COMMAND);
    if (auto const awaiting = m_instance.GetAwaiting().lock()) {
        awaiting->PushResponse(message);
    }

    // Need to track encryption keys and nonces
    NodeUtils::NodeIdType id = 0;
    if (auto const selfState = m_state.lock()->GetSelfState().lock()) {
        id = selfState->GetId();
    }

    NodeUtils::NodeIdType const& destinationId = message.GetSourceId();
    NodeUtils::NetworkNonce const nonce = message.GetNonce() + 1;
    CMessage const response(
        id,
        destinationId,
        NodeUtils::CommandType::QUERY,
        static_cast<std::uint32_t>(Phase::CLOSE),
        "Message Response",
        nonce);

    if (auto const messageQueue = m_instance.GetMessageQueue().lock()) {
        messageQueue->PushOutgoingMessage(destinationId, response);
        messageQueue->PushOutgoingMessages();
        m_instance.NotifyConnection(destinationId);
    }
    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the close phase for the Query type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQuery::CloseHandler()
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
