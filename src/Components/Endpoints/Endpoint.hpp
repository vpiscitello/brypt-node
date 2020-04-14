//------------------------------------------------------------------------------------------------
// File: Endpoint.hpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports ZMQ D, ZMQ StreamBridge, and TCP sockets.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Configuration/Configuration.hpp"
#include "../../Interfaces/MessageSink.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "zmq.hpp"
//------------------------------------------------------------------------------------------------
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <thread>
#include <string>
//------------------------------------------------------------------------------------------------

class CMessage;

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Endpoints {
//------------------------------------------------------------------------------------------------

class CDirectEndpoint;
class CLoRaEndpoint;
class CStreamBridgeEndpoint;
class CTcpEndpoint;

std::shared_ptr<CEndpoint> Factory(
    IMessageSink* const messageSink,
    Configuration::TEndpointOptions const& options);

//------------------------------------------------------------------------------------------------
} // Command namespace
//------------------------------------------------------------------------------------------------

class CEndpoint {
public:
    CEndpoint(
        IMessageSink* const messageSink,
        Configuration::TEndpointOptions const& options)
        : m_mutex()
        , m_operation(options.operation)
        , m_id(options.id)
        , m_messageSink(messageSink)
        , m_active(false)
        , m_terminate(false)
        , m_cv()
        , m_worker()
    {
        if (m_operation == NodeUtils::EndpointOperation::None) {
            throw std::runtime_error("Endpoint must be provided and endpoint operation type!");
        }
    };

    virtual ~CEndpoint() {}; 

    virtual NodeUtils::TechnologyType GetInternalType() const = 0;
    virtual std::string GetProtocolType() const = 0;

    virtual void Startup() = 0;

    virtual void HandleProcessedMessage(NodeUtils::NodeIdType id, CMessage const& message) = 0;
	virtual void Send(NodeUtils::NodeIdType id, CMessage const& message) = 0;
	virtual void Send(NodeUtils::NodeIdType id, std::string_view message) = 0;
    
	virtual bool Shutdown() = 0;

    //------------------------------------------------------------------------------------------------

    bool GetStatus() const
    {
        return m_active;
    }

    //------------------------------------------------------------------------------------------------

    NodeUtils::EndpointOperation GetOperation() const
    {
        return m_operation;
    }

    //------------------------------------------------------------------------------------------------
    
protected:
    mutable std::mutex m_mutex;

	NodeUtils::EndpointOperation const m_operation;
	NodeUtils::NodeIdType const m_id;
    IMessageSink* const m_messageSink;

	std::atomic_bool m_active;
    std::atomic_bool m_terminate;
    mutable std::condition_variable m_cv;
    std::thread m_worker;

};

//------------------------------------------------------------------------------------------------
