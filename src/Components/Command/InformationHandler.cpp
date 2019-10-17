//------------------------------------------------------------------------------------------------
// File: InformationHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "InformationHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Await/Await.hpp"
#include "../Connection/Connection.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------
std::string GenerateNodeInfo(CNode const& node, std::weak_ptr<CState> const& state);
//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Command::CInformation::CInformation(CNode& instance, std::weak_ptr<CState> const& state)
    : CHandler(instance, state)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
void Command::CInformation::whatami()
{
    printo("Handling response to Information request", NodeUtils::PrintType::COMMAND);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Information message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformation::HandleMessage(CMessage const& message)
{
    bool status = false;
    whatami();

    auto const phase = static_cast<CInformation::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::FLOOD: {
            status = FloodHandler(message);
        } break;
        case Phase::RESPOND: {
            status = RespondHandler();
        } break;
        case Phase::CLOSE: {
            status = CloseHandler();
        } break;
        default: break;
    }

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the flood phase for the Information type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformation::FloodHandler(CMessage const& message)
{
    printo("Building response for Information request", NodeUtils::PrintType::COMMAND);
    
    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = std::string();
    if (auto const selfState = m_state.lock()->GetSelfState().lock()) {
        id = selfState->GetId();
    }

    NodeUtils::NetworkNonce const nonce = 0;
    CMessage const infoMessage(
        id,
        message.GetSourceId(),
        NodeUtils::CommandType::INFORMATION,
        static_cast<std::uint32_t>(Phase::RESPOND),
        local::GenerateNodeInfo(m_instance, m_state),
        nonce
    );

    if (auto const awaiting = m_instance.GetAwaiting().lock()) {
        auto const awaitKey = awaiting->PushRequest(message, id, 1);
        awaiting->PushResponse(awaitKey, infoMessage);
    }

    // TODO: Add notification distribution, so branches can add their information

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the respond phase for the Information type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformation::RespondHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the close phase for the Information type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CInformation::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: This constructs a JSON object for each of the messages from each of the connections.
// Returns: The JSON structure as a string.
//------------------------------------------------------------------------------------------------
std::string local::GenerateNodeInfo(CNode const& m_instance, std::weak_ptr<CState> const& state)
{
    std::vector<json11::Json::object> nodesInfo;

    // Get the information pertaining to the node itself
    NodeUtils::NodeIdType id = std::string();
    std::string cluster = std::string();
    NodeUtils::DeviceOperation operation = NodeUtils::DeviceOperation::NONE;
    if (auto const selfState = state.lock()->GetSelfState().lock()) {
        id = selfState->GetId();
        cluster = selfState->GetCluster();
        operation = selfState->GetOperation();
    }

    // Get the information pertaining to the node's coordinator
    NodeUtils::NodeIdType coordinatorId = std::string();
    if (auto const coordinatorState = state.lock()->GetCoordinatorState().lock()) {
        coordinatorId = coordinatorState->GetId();
    }

    // Get the information pertaining to the node's network
    std::size_t knownNodes = 0;
    if (auto const networkState = state.lock()->GetNetworkState().lock()) {
        knownNodes = networkState->GetKnownNodes();
    }

    nodesInfo.emplace_back(
        json11::Json::object {
            {"uid", id},
            {"cluster", cluster},
            {"coordinator", coordinatorId},
            {"neighbor_count", static_cast<std::int32_t>(knownNodes)},
            {"designation", NodeUtils::GetDesignation(operation)},
            {"technologies", json11::Json::array{"IEEE 802.11"}},
            {"update_timestamp", NodeUtils::GetSystemTimestamp()}
        }
    );

    if (auto const connections = m_instance.GetConnections().lock()) {
        for (auto itr = connections->begin(); itr != connections->end(); ++itr) {
            std::string const timestamp = NodeUtils::TimePointToString(itr->second->GetUpdateClock());
            nodesInfo.emplace_back(
                json11::Json::object {
                    {"uid", itr->second->GetPeerName()},
                    {"cluster", cluster},
                    {"coordinator", id},
                    {"neighbor_count", 0},
                    {"designation", "node"},
                    {"technologies", json11::Json::array{itr->second->GetProtocolType()}},
                    {"update_timestamp", timestamp}
                }
            );
        }
    }

    json11::Json nodesJson = json11::Json::array({nodesInfo});

    return nodesJson[0].dump();
}

//------------------------------------------------------------------------------------------------
