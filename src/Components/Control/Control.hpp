//------------------------------------------------------------------------------------------------
// File: Control.hpp
// Description: Defines operations for the control thread. Currently only handles requests for 
// new connections
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
#include <optional>
#include <sys/socket.h>
//------------------------------------------------------------------------------------------------
class CConnection;
class CState;
//------------------------------------------------------------------------------------------------

class CControl {
public:
    CControl(
        std::shared_ptr<CState> const& state,
        std::weak_ptr<NodeUtils::ConnectionMap> const& connections,
        NodeUtils::TechnologyType const& technology);

    void Send(CMessage const& message);
    void Send(char const* const message);
    std::optional<std::string> HandleRequest();
    std::optional<std::string> HandleContact(NodeUtils::TechnologyType technology);
    void CloseCurrentConnection();

private:
    std::shared_ptr<CState> m_state;
    std::weak_ptr<NodeUtils::ConnectionMap> m_connections;
    std::shared_ptr<CConnection> m_control;
};

//------------------------------------------------------------------------------------------------
