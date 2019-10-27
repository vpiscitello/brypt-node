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
//------------------------------------------------------------------------------------------------
// Description: Handle Requests regarding Connecting to a new network or peer
//------------------------------------------------------------------------------------------------
std::unique_ptr<Command::CHandler> Command::Factory(
    NodeUtils::CommandType command,
    CNode& instance,
    std::weak_ptr<CState> const& state)
{
    switch (command) {
    	case NodeUtils::CommandType::CONNECT: return std::make_unique<CConnect>(instance, state);
    	case NodeUtils::CommandType::ELECTION: return std::make_unique<CElection>(instance, state);
    	case NodeUtils::CommandType::INFORMATION: return std::make_unique<CInformation>(instance, state);
    	case NodeUtils::CommandType::QUERY: return std::make_unique<CQuery>(instance, state);
    	case NodeUtils::CommandType::TRANSFORM: return std::make_unique<CTransform>(instance, state);
    	case NodeUtils::CommandType::NONE: return nullptr;
    }
	return nullptr;
}

//------------------------------------------------------------------------------------------------