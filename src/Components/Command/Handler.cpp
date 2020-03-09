//------------------------------------------------------------------------------------------------
// File: ConnectHandler.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Handler.hpp"
//------------------------------------------------------------------------------------------------
#include "ConnectHandler.hpp"
#include "ElectionHandler.hpp"
#include "InformationHandler.hpp"
#include "QueryHandler.hpp"
#include "TransformHandler.hpp"
#include "../BryptNode/BryptNode.hpp"
//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Connecting to a new network or peer
//------------------------------------------------------------------------------------------------

std::unique_ptr<Command::IHandler> Command::Factory(
    Command::Type commandType,
    CBryptNode& instance)
{
    switch (commandType) {
        case Command::Type::Connect:
            return std::make_unique<CConnectHandler>(instance);

        case Command::Type::Election:
            return std::make_unique<CElectionHandler>(instance);

        case Command::Type::Information:
            return std::make_unique<CInformationHandler>(instance);

        case Command::Type::Query:
            return std::make_unique<CQueryHandler>(instance);

        case Command::Type::Transform:
            return std::make_unique<CTransformHandler>(instance);

        case Command::Type::Invalid:
        default:
            return {};
    }
}

//------------------------------------------------------------------------------------------------
// IHandler implementation
//------------------------------------------------------------------------------------------------

Command::IHandler::IHandler(
    Command::Type type,
    CBryptNode& instance)
    : m_type(type)
    , m_instance(instance)
{
}

//------------------------------------------------------------------------------------------------

Command::Type Command::IHandler::GetType() const
{
    return m_type;
}

//------------------------------------------------------------------------------------------------