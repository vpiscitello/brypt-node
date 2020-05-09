//------------------------------------------------------------------------------------------------
// File: Handler.hpp
// Description: Defines a set of command types for messages and the appropriate responses based
// on the phase that the communication is currently in.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "CommandDefinitions.hpp"
#include "../../Utilities/Message.hpp"
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <string>
//------------------------------------------------------------------------------------------------

class CBryptNode;

//------------------------------------------------------------------------------------------------
namespace Command {
//------------------------------------------------------------------------------------------------
class IHandler;

class CConnectHandler;
class CElectionHandler;
class CInformationHandler;
class CQueryHandler;
class CTransformHandler;

std::unique_ptr<IHandler> Factory(
    Command::Type commandType,
    CBryptNode& instance);

using HandlerMap = std::unordered_map<Command::Type, std::unique_ptr<IHandler>>;

//------------------------------------------------------------------------------------------------
} // Command namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description:
//------------------------------------------------------------------------------------------------

class Command::IHandler {
public:
    IHandler(
        Command::Type type,
        CBryptNode& instance);
    virtual ~IHandler() = default;
    
    virtual Command::Type GetType() const final;

    virtual bool HandleMessage(CMessage const& message) = 0;

protected:
    Command::Type m_type;
    CBryptNode& m_instance;
};

//------------------------------------------------------------------------------------------------
