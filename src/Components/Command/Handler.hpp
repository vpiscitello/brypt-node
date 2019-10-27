//------------------------------------------------------------------------------------------------
// File: Handler.hpp
// Description: Defines a set of command types for messages and the appropriate responses based
// on the phase that the communication is currently in.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Notifier/Notifier.hpp"
#include "../../Node.hpp"
#include "../../State.hpp"
#include "../../Utilities/Message.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <string>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Command {
//------------------------------------------------------------------------------------------------
class CHandler;

class CConnect;
class CElection;
class CInformation;
class CQuery;
class CTransform;

std::unique_ptr<CHandler> Factory(
    NodeUtils::CommandType command,
    CNode& instance,
    std::weak_ptr<CState> const& state);
//------------------------------------------------------------------------------------------------
} // Command namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------
class Command::CHandler {
public:
    CHandler(
        CNode& instance,
        std::weak_ptr<CState> const& state)
        : m_instance(instance)
        , m_state(state)
    {
    };
    virtual ~CHandler() {};
    virtual void whatami() = 0;
    virtual bool HandleMessage(CMessage const& CMessage) = 0;

protected:
    CNode& m_instance;
    std::weak_ptr<CState> m_state;
};

//------------------------------------------------------------------------------------------------
