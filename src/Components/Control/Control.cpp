//------------------------------------------------------------------------------------------------
// File: Contol.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Control.hpp"
#include "../Connection/Connection.hpp"
#include "../../State.hpp"
#include "../../Utilities/Message.hpp"
//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------

CControl::CControl(
    std::shared_ptr<CState> const& state,
    IMessageSink* const messageSink,
    std::weak_ptr<NodeUtils::ConnectionMap> const& connections,
    NodeUtils::TechnologyType technology)
    : m_state(state)
    , m_connections(connections)
    , m_control()
{
    Configuration::TConnectionOptions options;
    options.technology = technology;
    options.operation = NodeUtils::ConnectionOperation::Server;
    if (auto const selfState = m_state->GetSelfState().lock()) {
        options.binding = selfState->GetBinding();
    }

    m_control = Connection::Factory(messageSink, options);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Passthrough for the send function for the particular communication type used
//------------------------------------------------------------------------------------------------
void CControl::Send(CMessage const& message)
{
    m_control->Send(message);
}

//------------------------------------------------------------------------------------------------
// Description: Passthrough for the send function for the particular communication type used.
//-----------------------------------------------------------------------------------------------
void CControl::Send(std::string_view message)
{
    m_control->Send(message);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Handles new requests. Operates in nonblocking mode: if there is a request present,
// enters blocking mode and handles the request.
//-----------------------------------------------------------------------------------------------
std::optional<std::string> CControl::HandleRequest()
{
    std::optional<std::string> optRequest = m_control->Receive(ZMQ_NOBLOCK);
    if (!optRequest) { return {}; }

    switch (optRequest->size()) {
        case 0: return {};
        case 1: {
            NodeUtils::printo("Received connection byte", NodeUtils::PrintType::Control);
            if (*optRequest == "\x06") {
                NodeUtils::printo("Device connection acknowledgement", NodeUtils::PrintType::Control);

                m_control->Send("\x06");
                NodeUtils::printo("Device was sent acknowledgement", NodeUtils::PrintType::Control);

                optRequest = m_control->Receive(0);
                if (!optRequest) { return {}; }
                std::cout << "Request was " << *optRequest << "\n";

                std::int8_t const technologyTypeByte = static_cast<std::int8_t>(optRequest->at(0) - 48);
                if (technologyTypeByte > -1 && technologyTypeByte < 4) {
                    NodeUtils::printo("Communication type requested: " + std::to_string(technologyTypeByte), NodeUtils::PrintType::Control);
                    NodeUtils::TechnologyType technology = static_cast<NodeUtils::TechnologyType>(technologyTypeByte);
                    if (technology == NodeUtils::TechnologyType::TCP) {
                        technology = NodeUtils::TechnologyType::StreamBridge;
                    }
                    return HandleContact(technology);
                } else {
                    NodeUtils::printo("Somethings not right", NodeUtils::PrintType::Control);
                    m_control->Send("\x15");
                }
            }
            return {};
        }
        default: {
            CMessage const message(*optRequest);
            return message.GetPack();
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: This handles the finishing of a request for a new connection type
//-----------------------------------------------------------------------------------------------
std::optional<std::string> CControl::HandleContact(NodeUtils::TechnologyType technology)
{
    printo("Handling request from control socket", NodeUtils::PrintType::Control);

    switch (technology) {
        case NodeUtils::TechnologyType::TCP:
        case NodeUtils::TechnologyType::StreamBridge:
        case NodeUtils::TechnologyType::Direct: {
            NodeUtils::NodeIdType id = 0;
            NodeUtils::PortNumber port;
            if (auto const selfState = m_state->GetSelfState().lock()) {
                id = selfState->GetId();
                port = std::to_string(selfState->GetNextPort());
            }

            NodeUtils::printo("Sending port: " + port, NodeUtils::PrintType::Control);
            CMessage message(id, 0xFFFFFFFF, NodeUtils::CommandType::Connect, 0, port, 0);
            m_control->Send(message);

            std::optional<std::string> const optDeviceInfoMessage = m_control->Receive(0);
            if (!optDeviceInfoMessage) { return {}; }

            NodeUtils::printo("Received: " + *optDeviceInfoMessage, NodeUtils::PrintType::Control);
            return optDeviceInfoMessage;
        }
        default: {
            m_control->Send("\x15");
        }
    }

    return {};
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Special for the TCP connection type, it calls the connection's internal 
// PrepareForNext function which cleans up the current socket and prepares for a new connection
//-----------------------------------------------------------------------------------------------
void CControl::CloseCurrentConnection()
{
    if (m_control->GetInternalType() == NodeUtils::TechnologyType::TCP) {
        m_control->PrepareForNext();
    }
}

//------------------------------------------------------------------------------------------------
