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
class CState;

class CMessage;
//------------------------------------------------------------------------------------------------

class CControl {
public:
    CControl(
        std::shared_ptr<CState> const& state,
        IMessageSink* const messageSink,
        std::weak_ptr<NodeUtils::ConnectionMap> const& connections,
        NodeUtils::TechnologyType technology);

    void Send(CMessage const& message);
   void Send(std::string_view message);
    std::optional<std::string> HandleRequest();
    std::optional<std::string> HandleContact(NodeUtils::TechnologyType technology);
    void CloseCurrentConnection();

private:
    std::shared_ptr<CState> m_state;
    std::weak_ptr<NodeUtils::ConnectionMap> m_connections;
    std::shared_ptr<Connection::CTcp> m_control;
};

//------------------------------------------------------------------------------------------------
