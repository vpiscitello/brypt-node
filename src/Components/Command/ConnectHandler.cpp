//------------------------------------------------------------------------------------------------
// File: ConnectHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ConnectHandler.hpp"
//------------------------------------------------------------------------------------------------
#include "../Control/Control.hpp"
#include "../Connection/Connection.hpp"
#include <chrono>
#include <thread>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
Command::CConnect::CConnect(CNode& instance, std::weak_ptr<CState> const& state)
    : CHandler(instance, state, NodeUtils::CommandType::CONNECT)
{
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Connect message handler, drives each of the message responses based on the phase
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnect::HandleMessage(CMessage const& message)
{
    bool status = false;

    auto const phase = static_cast<CConnect::Phase>(message.GetPhase());
    switch (phase) {
        case Phase::CONTACT: {
            status = ContactHandler();
        } break;
        case Phase::JOIN: {
            status = JoinHandler(message);
        } break;
        case Phase::CLOSE: {
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
bool Command::CConnect::ContactHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles the join phase for the Connect type command
// Returns: Status of the message handling
//------------------------------------------------------------------------------------------------
bool Command::CConnect::JoinHandler(CMessage const& message)
{
    printo("Setting up full connection", NodeUtils::PrintType::COMMAND);

    // Get the new port to host the connection on
    NodeUtils::PortNumber port = std::string();
    if (auto const selfState = m_state.lock()->GetSelfState().lock()) {
        port = std::to_string(selfState->GetNextPort());
    }

    // Get the requested type of connection
    Message::Buffer const& buffer = message.GetData();
    auto technology = static_cast<NodeUtils::TechnologyType>(buffer.front());
    if (technology == NodeUtils::TechnologyType::TCP) {
        technology = NodeUtils::TechnologyType::STREAMBRIDGE;
    }

    // Setup the new connection for the node
    std::shared_ptr<CConnection> const connection =
        m_instance.SetupFullConnection(message.GetSourceId(), port, technology);

    // Push the node's id into the network peer names set
    if (auto const networkState = m_state.lock()->GetNetworkState().lock()) {
        networkState->PushPeerName(message.GetSourceId());
    }

    // Push the new connection into the map of managed connections
    std::shared_ptr<NodeUtils::ConnectionMap> const& connections = m_instance.GetConnections().lock();
    connections->emplace(message.GetSourceId(), connection);
    printo("New connection pushed back", NodeUtils::PrintType::COMMAND);

    // Wait for the connection to be fully setup
    while (!connection->GetStatus()) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }
    
    // Notify the node that the new managed connection is ready
    printo("Connection worker thread is ready", NodeUtils::PrintType::COMMAND);
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
bool Command::CConnect::CloseHandler()
{
    return false;
}

//------------------------------------------------------------------------------------------------
