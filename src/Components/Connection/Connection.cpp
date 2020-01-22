//------------------------------------------------------------------------------------------------
// File: Connection.cpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports ZMQ REQ/REP, ZMQ StreamBridge, and TCP sockets.
//------------------------------------------------------------------------------------------------
#include "Connection.hpp"
//------------------------------------------------------------------------------------------------
#include "DirectConnection.hpp"
#include "LoRaConnection.hpp"
#include "StreamBridgeConnection.hpp"
#include "TcpConnection.hpp"
#include "../../Utilities/Message.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

std::shared_ptr<CConnection> Connection::Factory(
    IMessageSink* const messageSink,
    Configuration::TConnectionOptions const& options)
{
    switch (options.technology) {
        case NodeUtils::TechnologyType::DIRECT: return std::make_shared<CDirect>(messageSink, options);
        case NodeUtils::TechnologyType::LORA: return std::make_shared<CLoRa>(messageSink, options);
        case NodeUtils::TechnologyType::STREAMBRIDGE: return std::make_shared<CStreamBridge>(messageSink, options);
        case NodeUtils::TechnologyType::TCP: return std::make_shared<CTcp>(messageSink, options);
        case NodeUtils::TechnologyType::NONE: return nullptr;
    }
    return nullptr;
}

//------------------------------------------------------------------------------------------------
