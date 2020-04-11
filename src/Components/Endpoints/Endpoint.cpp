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

std::shared_ptr<CEndpoint> Endpoints::Factory(
    IMessageSink* const messageSink,
    Configuration::TEndpointOptions const& options)
{
    switch (options.technology) {
        case NodeUtils::TechnologyType::Direct: {
            return std::make_shared<CDirectEndpoint>(messageSink, options);
        }
        case NodeUtils::TechnologyType::LoRa: {
            return std::make_shared<CLoRaEndpoint>(messageSink, options);
        }
        case NodeUtils::TechnologyType::StreamBridge: {
            return std::make_shared<CStreamBridgeEndpoint>(messageSink, options);
        }
        case NodeUtils::TechnologyType::TCP: {
            return std::make_shared<CTcpEndpoint>(messageSink, options);
        }
        case NodeUtils::TechnologyType::None: return nullptr;
    }
    return nullptr;
}

//------------------------------------------------------------------------------------------------
