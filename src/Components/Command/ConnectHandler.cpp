//------------------------------------------------------------------------------------------------
// File: ConnectHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ConnectHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Control/Control.hpp"
#include "../Connection/Connection.hpp"
#include "../BryptNode/BryptNode.hpp"
#include "../BryptNode/NetworkState.hpp"
#include "../BryptNode/NodeState.hpp"
#include <chrono>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Command::CConnectHandler::CConnectHandler(CBryptNode& instance)
    : IHandler(Command::Type::Connect, instance)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Connect message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::HandleMessage(CMessage const& message)
{
    bool status = false;

    auto const phase = static_cast<CConnectHandler::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::Contact: {
            status = ContactHandler();
        } break;
        case Phase::Join: {
            status = JoinHandler(message);
        } break;
        case Phase::Close: {
            status = CloseHandler();
        } break;
        default: {
            if (auto const control = m_instance.GetControl().lock()) {
                control->Send("\x15");
            }
        } break;
    }

    return status;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::ContactHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the join phase for the Connect type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::JoinHandler(CMessage const& message)
{
    printo("Setting up full connection", NodeUtils::PrintType::Command);

    // Get the new port to host the connection on
    NodeUtils::PortNumber port = std::string();
    if (auto const spNodeState = m_instance.GetNodeState().lock()) {
        port = std::to_string(spNodeState->GetNextPort());
    }

    // Get the requested type of connection
    Message::Buffer const& buffer = message.GetData();
    auto technology = static_cast<NodeUtils::TechnologyType>(buffer.front());
    if (technology == NodeUtils::TechnologyType::TCP) {
        technology = NodeUtils::TechnologyType::StreamBridge;
    }

    // Setup the new connection for the node
    std::shared_ptr<CConnection> const connection =
        m_instance.SetupFullConnection(message.GetSourceId(), port, technology);

    // Push the node's id into the network peer names set
    if (auto const spNetworkState = m_instance.GetNetworkState().lock()) {
        spNetworkState->PushPeerName(message.GetSourceId());
    }

    // Push the new connection into the map of managed connections
    std::shared_ptr<NodeUtils::ConnectionMap> const& connections = m_instance.GetConnections().lock();
    connections->emplace(message.GetSourceId(), connection);
    printo("New connection pushed back", NodeUtils::PrintType::Command);

    // Wait for the connection to be fully setup
    while (!connection->GetStatus()) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
    
    // Notify the node that the new managed connection is ready
    printo("Connection worker thread is ready", NodeUtils::PrintType::Command);
    if (auto const control = m_instance.GetControl().lock()) {
        control->Send("\x04");
    }

    return true;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnectHandler::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------
