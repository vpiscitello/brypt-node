//------------------------------------------------------------------------------------------------
// File: Connection.hpp
// Description: Defines a set of communication methods for use on varying types of communication
// technologies. Currently supports ZMQ D, ZMQ StreamBridge, and TCP sockets.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
#include "zmq.hpp"
//------------------------------------------------------------------------------------------------
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

std::shared_ptr<CConnection> Factory(NodeUtils::TechnologyType const& technology);
std::shared_ptr<CConnection> Factory(NodeUtils::TOptions const& options);
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
    CConnection()
        : m_active(false)
        , m_instantiate()
        , m_operation(NodeUtils::DeviceOperation::NONE)
        , m_peerName(std::string())
        , m_pipeName(std::string())
        , m_pipe()
        , m_sequenceNumber(0)
        , m_updateClock()
        , m_workerActive(false)
        , m_responseNeeded(false)
        , m_worker()
        , m_workerMutex()
        , m_workerConditional()
    {
    };

    explicit CConnection(NodeUtils::TOptions const& options)
        : m_active(false)
        , m_instantiate()
        , m_operation(options.m_operation)
        , m_peerName(options.m_peerName)
        , m_pipeName(std::string())
        , m_pipe()
        , m_sequenceNumber(0)
        , m_updateClock(NodeUtils::GetSystemTimePoint())
        , m_workerActive(false)
        , m_responseNeeded(false)
        , m_worker()
        , m_workerMutex()
        , m_workerConditional()
    {
        if (m_operation == NodeUtils::DeviceOperation::NONE) {
            throw std::runtime_error("Direct connection must be provided and device operation type!");
        }
    };

    virtual ~CConnection() {};

	virtual void whatami() = 0;
    virtual NodeUtils::TechnologyType const& GetInternalType() const = 0;
    virtual std::string const& GetProtocolType() const = 0;

	virtual void Spawn() = 0;
	virtual void Worker() = 0;

	virtual void Send(CMessage const& message) = 0;
	virtual void Send(char const* const message) = 0;
	virtual std::optional<std::string> Receive(std::int32_t flag = 0) = 0;
    
    virtual void PrepareForNext() = 0;
	virtual void Shutdown() = 0;

    //------------------------------------------------------------------------------------------------

    bool GetStatus() const
    {
        return m_active;
    }

    //------------------------------------------------------------------------------------------------

    bool GetWorkerStatus() const
    {
        return m_workerActive;
    }

    //------------------------------------------------------------------------------------------------

    std::string const& GetPeerName() const
    {
        return m_peerName;
    }

    //------------------------------------------------------------------------------------------------

    std::string const& GetPipeName() const
    {
        return m_pipeName;
    }

    //------------------------------------------------------------------------------------------------

    NodeUtils::TimePoint GetUpdateClock() const
    {
        return m_updateClock;
    }

    //------------------------------------------------------------------------------------------------

    bool CreatePipe()
    {
        if (m_peerName.empty()) {
            return false;
        }

        std::string const filename = "./tmp/" + m_peerName + ".pipe";

        m_pipeName = filename;
        m_pipe.open(filename, std::fstream::in | std::fstream::out | std::fstream::trunc);

        if(m_pipe.fail()){
    		m_pipe.close();
    		return false;
        }

        return true;
    }

    //------------------------------------------------------------------------------------------------

    bool WriteToPipe(std::string const& message)
    {
        if (!m_pipe.good()) {
            return false;
        }

        printo("Writing \"" + message + "\" to pipe", NodeUtils::PrintType::CONNECTION);
        m_pipe.clear();
        m_pipe.seekp(0);
        m_pipe << message << std::endl;
        m_pipe.flush();
        return true;
    }

    //------------------------------------------------------------------------------------------------

    std::string ReadFromPipe()
    {
        std::string raw = std::string();

        if (!m_pipe.good()) {
            printo("Pipe file is not good", NodeUtils::PrintType::CONNECTION);
            return raw;
        }
        m_pipe.clear();
        m_pipe.seekg(0);

        if (m_pipe.eof()) {
            printo("Pipe file is at the EOF", NodeUtils::PrintType::CONNECTION);
            return raw;
        }

        std::getline(m_pipe, raw);
        m_pipe.clear();

        printo("Sending " + raw, NodeUtils::PrintType::CONNECTION);

        std::ofstream clear_file(m_pipeName, std::ios::out | std::ios::trunc);
        clear_file.close();

        return raw;
    }

    //------------------------------------------------------------------------------------------------

    void ResponseReady(NodeUtils::NodeIdType const& id)
    {
        if (m_peerName != id) {
            printo("Response was not for this peer", NodeUtils::PrintType::CONNECTION);
            return;
        }

        m_workerActive = false;
        std::unique_lock<std::mutex> threadLock(m_workerMutex);
        m_workerConditional.notify_one();
        threadLock.unlock();
    }

    //------------------------------------------------------------------------------------------------

protected:
	bool m_active;
	bool m_instantiate;
	NodeUtils::DeviceOperation m_operation;

	NodeUtils::NodeIdType m_peerName;
	std::string m_pipeName;
	std::fstream m_pipe;
	std::uint32_t m_sequenceNumber;

    NodeUtils::TimePoint m_updateClock;

    bool m_workerActive;
    bool m_responseNeeded;
    std::thread m_worker;
    std::mutex m_workerMutex;
    std::condition_variable m_workerConditional;

};

//------------------------------------------------------------------------------------------------
