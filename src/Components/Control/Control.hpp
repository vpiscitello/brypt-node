//------------------------------------------------------------------------------------------------
// File: Control.hpp
// Description: Defines operations for the control thread. Currently only handles requests for 
// new connections
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Interfaces/MessageSink.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
#include <optional>
#include <sys/socket.h>
//------------------------------------------------------------------------------------------------
class CConnection;
class CNodeState;

class CMessage;
namespace Connection {
    class CTcp;
}

class CMessage;
namespace Connection {
    class CTcp;
}

//------------------------------------------------------------------------------------------------

class CControl {
public:
    CControl(
        std::weak_ptr<CNodeState> const& wpNodeState,
        IMessageSink* const messageSink,
        std::weak_ptr<NodeUtils::ConnectionMap> const& connections);

    void Send(CMessage const& message);
    void Send(std::string_view message);
    std::optional<std::string> HandleRequest();
    std::optional<std::string> HandleContact(NodeUtils::TechnologyType technology);
    void CloseCurrentConnection();

private:
    std::weak_ptr<CNodeState> m_wpNodeState;
    std::weak_ptr<NodeUtils::ConnectionMap> m_connections;
    std::shared_ptr<Connection::CTcp> m_control;
};

//------------------------------------------------------------------------------------------------
