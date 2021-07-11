//----------------------------------------------------------------------------------------------------------------------
// File: Query.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Query.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptMessage/ApplicationMessage.hpp"
#include "BryptNode/BryptNode.hpp"
#include "BryptNode/NodeState.hpp"
#include "BryptNode/NetworkState.hpp"
#include "Components/Await/TrackingManager.hpp"
#include "Components/Network/Endpoint.hpp"
#include "Utilities/TimeUtils.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <lithium_json.hh>
//----------------------------------------------------------------------------------------------------------------------
#include <set>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
//----------------------------------------------------------------------------------------------------------------------
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::string GenerateReading();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace Json {
//----------------------------------------------------------------------------------------------------------------------

struct Reading;

//----------------------------------------------------------------------------------------------------------------------
} // Json namespace
//----------------------------------------------------------------------------------------------------------------------
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding
//----------------------------------------------------------------------------------------------------------------------
#ifndef LI_SYMBOL_reading
#define LI_SYMBOL_reading
LI_SYMBOL(reading)
#endif
#ifndef LI_SYMBOL_timestamp
#define LI_SYMBOL_timestamp
LI_SYMBOL(timestamp)
#endif
//----------------------------------------------------------------------------------------------------------------------

struct Json::Reading
{
    Reading(
        std::uint32_t reading,
        TimeUtils::Timestamp const& timestamp)
        : reading(reading)
        , timestamp(timestamp.count())
    {
    }
    std::uint32_t const reading;
    std::uint64_t const timestamp;
};

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description:
//----------------------------------------------------------------------------------------------------------------------
Handler::Query::Query(BryptNode& instance)
    : IHandler(Handler::Type::Query, instance)
{
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Information message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//----------------------------------------------------------------------------------------------------------------------
bool Handler::Query::HandleMessage(AssociatedMessage const& associatedMessage)
{
    bool status = false;

    auto& [wpPeerProxy, message] = associatedMessage;
    auto const phase = static_cast<Query::Phase>(message.GetPhase());
    switch (phase) {
        case Query::Phase::Flood: { status = FloodHandler(wpPeerProxy, message); } break;
        case Query::Phase::Respond: { status = RespondHandler(wpPeerProxy, message); } break;
        case Query::Phase::Aggregate: { status = AggregateHandler(wpPeerProxy, message); } break;
        case Query::Phase::Close: { status = CloseHandler(); } break;
        default: break;
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Handles the flood phase for the Query type handler
// Returns: Status of the message handling
//----------------------------------------------------------------------------------------------------------------------
bool Handler::Query::FloodHandler(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& message)
{
    m_logger->debug(
        "Flooding query request in service for {}", message.GetSourceIdentifier());

    IHandler::SendClusterNotice(
        wpPeerProxy, message,
        "Request for Sensor Readings.",
        static_cast<std::uint8_t>(Phase::Respond),
        static_cast<std::uint8_t>(Phase::Aggregate),
        local::GenerateReading());
        
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Handles the respond phase for the Query type handler
// Returns: Status of the message handling
//----------------------------------------------------------------------------------------------------------------------
bool Handler::Query::RespondHandler(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& message)
{
    m_logger->debug(
        "Building response for the Query request from {}.", message.GetSourceIdentifier());
    IHandler::SendResponse(
        wpPeerProxy, message, local::GenerateReading(), static_cast<std::uint8_t>(Phase::Aggregate));
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Handles the aggregate phase for the Query type handler
// Returns: Status of the message handling
//----------------------------------------------------------------------------------------------------------------------
bool Handler::Query::AggregateHandler(
    std::weak_ptr<Peer::Proxy> const& wpPeerProxy,
    ApplicationMessage const& message)
{
    if (auto const spAwaitManager = m_instance.GetAwaitManager().lock()) {
        spAwaitManager->PushResponse(message);
    }

    IHandler::SendResponse(
        wpPeerProxy, message, "Response Acknowledged.", static_cast<std::uint8_t>(Phase::Close));
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Handles the close phase for the Query type handler
// Returns: Status of the message handling
//----------------------------------------------------------------------------------------------------------------------
bool Handler::Query::CloseHandler()
{
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Generate a pseudo-random value for the temperature reading and pack it into JSON
// Returns: The JSON structure as a string
//----------------------------------------------------------------------------------------------------------------------
std::string local::GenerateReading()
{
    std::int32_t const value = rand() % ( 74 - 68 ) + 68;
    Json::Reading const reading(value, TimeUtils::GetSystemTimestamp());
    return li::json_object(s::reading, s::timestamp).encode(reading);
}

//----------------------------------------------------------------------------------------------------------------------
