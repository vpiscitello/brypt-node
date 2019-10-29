//------------------------------------------------------------------------------------------------
// File: Contol.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Control.hpp"
#include "../Connection/Connection.hpp"
#include "../../State.hpp"
#include "../../Utilities/ByteMessage.hpp"
//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------

CControl::CControl(
    std::shared_ptr<CState> const& state,
    std::weak_ptr<NodeUtils::ConnectionMap> const& connections,
    NodeUtils::TechnologyType technology)
    : m_state(state)
    , m_connections(connections)
    , m_control()
{
    NodeUtils::TOptions options;
    options.m_technology = technology;
    options.m_operation = NodeUtils::DeviceOperation::ROOT;
    options.m_isControl = true;
    if (auto const selfState = m_state->GetSelfState().lock()) {
        options.m_port = selfState->GetPort();
    }

    m_control = Connection::Factory(options);
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
void CControl::Send(char const* const message)
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
            NodeUtils::printo("Received connection byte", NodeUtils::PrintType::CONTROL);
            if (*optRequest == "\x06") {
                NodeUtils::printo("Device connection acknowledgement", NodeUtils::PrintType::CONTROL);

                m_control->Send("\x06");
                NodeUtils::printo("Device was sent acknowledgement", NodeUtils::PrintType::CONTROL);

                optRequest = m_control->Receive(0);
                if (!optRequest) { return {}; }
                std::cout << "Request was " << *optRequest << "\n";

                std::int8_t const technologyTypeByte = static_cast<std::int8_t>(optRequest->at(0) - 48);
                if (technologyTypeByte > -1 && technologyTypeByte < 4) {
                    NodeUtils::printo("Communication type requested: " + std::to_string(technologyTypeByte), NodeUtils::PrintType::CONTROL);
                    NodeUtils::TechnologyType technology = static_cast<NodeUtils::TechnologyType>(technologyTypeByte);
                    if (technology == NodeUtils::TechnologyType::TCP) {
                        technology = NodeUtils::TechnologyType::STREAMBRIDGE;
                    }
                    return HandleContact(technology);
                } else {
                    NodeUtils::printo("Somethings not right", NodeUtils::PrintType::CONTROL);
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
    printo("Handling request from control socket", NodeUtils::PrintType::CONTROL);

    switch (technology) {
        case NodeUtils::TechnologyType::TCP:
        case NodeUtils::TechnologyType::STREAMBRIDGE:
        case NodeUtils::TechnologyType::DIRECT: {
            NodeUtils::NodeIdType id;
            NodeUtils::PortNumber port;
            if (auto const selfState = m_state->GetSelfState().lock()) {
                id = selfState->GetId();
                port = std::to_string(selfState->GetNextPort());
            }

            NodeUtils::printo("Sending port: " + port, NodeUtils::PrintType::CONTROL);
            CMessage message(id, 0xFFFFFFFF, NodeUtils::CommandType::CONNECT, 0, port, 0);
            m_control->Send(message);

            std::optional<std::string> const optDeviceInfoMessage = m_control->Receive(0);
            if (!optDeviceInfoMessage) { return {}; }

            NodeUtils::printo("Received: " + *optDeviceInfoMessage, NodeUtils::PrintType::CONTROL);
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
