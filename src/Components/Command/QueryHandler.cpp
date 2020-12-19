//------------------------------------------------------------------------------------------------
// File: QueryHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "QueryHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Await/TrackingManager.hpp"
#include "../Endpoints/Endpoint.hpp"
#include "../../BryptNode/BryptNode.hpp"
#include "../../BryptNode/NodeState.hpp"
#include "../../BryptNode/NetworkState.hpp"
#include "../../BryptMessage/ApplicationMessage.hpp"
#include "../../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wconversion"
#include "../../Libraries/metajson/metajson.hh"
#pragma GCC diagnostic pop
//------------------------------------------------------------------------------------------------
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
        TimeUtils::Timestamp const& timestamp)
        : reading(reading)
        , timestamp(timestamp.count())
    {
    }
    std::uint32_t const reading;
    std::uint64_t const timestamp;
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
bool Command::CQueryHandler::HandleMessage(AssociatedMessage const& associatedMessage)
{
    bool status = false;

    auto& [wpBryptPeer, message] = associatedMessage;
    auto const phase = static_cast<CQueryHandler::Phase>(message.GetPhase());
    switch (phase) {
        case CQueryHandler::Phase::Flood: {
            status = FloodHandler(wpBryptPeer, message);
        } break;
        case CQueryHandler::Phase::Respond: {
            status = RespondHandler(wpBryptPeer, message);
        } break;
        case CQueryHandler::Phase::Aggregate: {
            status = AggregateHandler(wpBryptPeer, message);
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
bool Command::CQueryHandler::FloodHandler(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CApplicationMessage const& message)
{
    printo("Sending notification for Query request", NodeUtils::PrintType::Command);

    IHandler::SendClusterNotice(
        wpBryptPeer, message,
        "Request for Sensor Readings.",
        static_cast<std::uint8_t>(Phase::Respond),
        static_cast<std::uint8_t>(Phase::Aggregate),
        local::GenerateReading());
        
    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the respond phase for the Query type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQueryHandler::RespondHandler(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CApplicationMessage const& message)
{
    printo("Building response for Query request", NodeUtils::PrintType::Command);
    IHandler::SendResponse(
        wpBryptPeer, message, local::GenerateReading(), static_cast<std::uint8_t>(Phase::Aggregate));
    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the aggregate phase for the Query type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CQueryHandler::AggregateHandler(
    std::weak_ptr<CBryptPeer> const& wpBryptPeer,
    CApplicationMessage const& message)
{
    printo("Pushing response to ResponseTracker", NodeUtils::PrintType::Command);
    if (auto const spAwaitManager = m_instance.GetAwaitManager().lock()) {
        spAwaitManager->PushResponse(message);
    }

    IHandler::SendResponse(
        wpBryptPeer, message, "Response Acknowledged.", static_cast<std::uint8_t>(Phase::Close));
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
    Json::TReading const reading(value, TimeUtils::GetSystemTimestamp());
    return iod::json_object(s::reading, s::timestamp).encode(reading);
}

//------------------------------------------------------------------------------------------------
