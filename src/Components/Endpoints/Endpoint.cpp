//------------------------------------------------------------------------------------------------
// File: Endpoint.cpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports ZMQ REQ/REP, ZMQ StreamBridge, and TCP sockets.
//------------------------------------------------------------------------------------------------
#include "Endpoint.hpp"
//------------------------------------------------------------------------------------------------
#include "DirectEndpoint.hpp"
#include "LoRaEndpoint.hpp"
#include "StreamBridgeEndpoint.hpp"
#include "TcpEndpoint.hpp"
#include "../../Utilities/Message.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

std::unique_ptr<CEndpoint> Endpoints::Factory(
    TechnologyType technology,
    NodeUtils::NodeIdType id,
    std::string_view interface,
    Endpoints::OperationType operation,
    IMessageSink* const messageSink)
{
    switch (technology) {
        case TechnologyType::Direct: {
            return std::make_unique<CDirectEndpoint>(id, interface, operation, messageSink);
        }
        case TechnologyType::LoRa: {
            return std::make_unique<CLoRaEndpoint>(id, interface, operation, messageSink);
        }
        case TechnologyType::StreamBridge: {
            return std::make_unique<CStreamBridgeEndpoint>(id, interface, operation, messageSink);
        }
        case TechnologyType::TCP: {
            return std::make_unique<CTcpEndpoint>(id, interface, operation, messageSink);
        }
        case TechnologyType::Invalid: return nullptr;
    }
    return nullptr;
}

//------------------------------------------------------------------------------------------------
