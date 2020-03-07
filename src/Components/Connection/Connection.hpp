//------------------------------------------------------------------------------------------------
// File: Connection.hpp
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
#include <cstdlib>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <memory>
#include <string>
#include <vector>
//------------------------------------------------------------------------------------------------

class CConnection;
class CMessage;

//------------------------------------------------------------------------------------------------
namespace Connection {
//------------------------------------------------------------------------------------------------

class CDirect;
class CLoRa;
class CStreamBridge;
class CTcp;

std::shared_ptr<CConnection> Factory(
    IMessageSink* const messageSink,
    Configuration::TConnectionOptions const& options);

//------------------------------------------------------------------------------------------------
} // Command namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

// TODO:
// * Drop connections if the connected node does not match the intended device
// * Maintain key and nonce state for connections

//------------------------------------------------------------------------------------------------

class direct_monitor_t : public zmq::monitor_t {
public:
    //------------------------------------------------------------------------------------------------
    // Description:
    //------------------------------------------------------------------------------------------------
    void on_event_connected(zmq_event_t const&, const char *) ZMQ_OVERRIDE {
        m_connected = true;
    }

    //------------------------------------------------------------------------------------------------
    // Description:
    //------------------------------------------------------------------------------------------------
    void on_event_closed(zmq_event_t const&, const char *) ZMQ_OVERRIDE {
        m_disconnected = true;
    }

    //------------------------------------------------------------------------------------------------
    // Description:
    //------------------------------------------------------------------------------------------------
    void on_event_disconnected(zmq_event_t const&, const char *) ZMQ_OVERRIDE {
        m_disconnected = true;
    }

    bool m_connected = false;
    bool m_disconnected = false;

};

//------------------------------------------------------------------------------------------------

class CConnection {
public:
    CConnection(
        IMessageSink* const messageSink,
        Configuration::TConnectionOptions const& options)
        : m_active(false)
        , m_operation(options.operation)
        , m_id(options.id)
        , m_messageSink(messageSink)
        , m_sequenceNumber(0)
        , m_updateTimePoint(NodeUtils::GetSystemTimePoint())
        , m_terminate(false)
        , m_mutex()
        , m_cv()
        , m_worker()
    {
        if (m_operation == NodeUtils::ConnectionOperation::None) {
            throw std::runtime_error("Direct connection must be provided and device operation type!");
        }
    };

    virtual ~CConnection()
    {
        m_messageSink->UnpublishCallback(m_id);
    };

	virtual void whatami() = 0;
    virtual NodeUtils::TechnologyType GetInternalType() const = 0;
    virtual std::string const& GetProtocolType() const = 0;

	virtual void Spawn() = 0;
	virtual void Worker() = 0;

    virtual void HandleProcessedMessage(CMessage const& message) = 0;
	virtual void Send(CMessage const& message) = 0;
	virtual void Send(std::string_view message) = 0;
	virtual std::optional<std::string> Receive(std::int32_t flag = 0) = 0;
    
    virtual void PrepareForNext() = 0;
	virtual bool Shutdown() = 0;

    //------------------------------------------------------------------------------------------------

    bool GetStatus() const
    {
        return m_active;
    }

    //------------------------------------------------------------------------------------------------

    NodeUtils::ConnectionOperation GetOperation() const
    {
        std::scoped_lock lock(m_mutex);
        return m_operation;
    }

    //------------------------------------------------------------------------------------------------

    NodeUtils::NodeIdType const& GetPeerName() const
    {
        std::scoped_lock lock(m_mutex);
        return m_id;
    }

    //------------------------------------------------------------------------------------------------

    NodeUtils::TimePoint GetUpdateClock() const
    {
        return m_updateTimePoint;
    }

    //------------------------------------------------------------------------------------------------

    void ResponseReady(NodeUtils::NodeIdType const& id)
    {
        std::scoped_lock lock(m_mutex);
        if (m_id != id) {
            printo("Response was not for this peer", NodeUtils::PrintType::Connection);
            return;
        }

        m_active = false;
        m_cv.notify_one();
    }

    //------------------------------------------------------------------------------------------------

protected:
	std::atomic_bool m_active;
	NodeUtils::ConnectionOperation m_operation;

	NodeUtils::NodeIdType m_id;

    IMessageSink* const m_messageSink;

	std::atomic_uint32_t m_sequenceNumber;
    std::atomic<NodeUtils::TimePoint> m_updateTimePoint;

    std::atomic_bool m_terminate;
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
    std::thread m_worker;

};

//------------------------------------------------------------------------------------------------
