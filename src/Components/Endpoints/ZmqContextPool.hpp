//------------------------------------------------------------------------------------------------
// File: ZmqContextPool.hpp
// Description: A singleton class to ensure a single ZMQ context exists throughout the application
// for any endpoint classes that use ZMQ sockets. Per the ZMQ documentation, ZMQ contexts are 
// threadsafe, however, sockets are not. 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <zmq.hpp>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class ZmqContextPool
{
public:
    static ZmqContextPool& Instance() {
        static ZmqContextPool instance;
        return instance;
    }

    ZmqContextPool(ZmqContextPool const&) = delete;
    void operator=(ZmqContextPool const&) = delete;

    std::shared_ptr<zmq::context_t> GetContext() { return m_spContext; }
    
private:
    ZmqContextPool()
    {
        m_spContext = std::make_shared<zmq::context_t>(1);
    }

    std::shared_ptr<zmq::context_t> m_spContext;
};

//------------------------------------------------------------------------------------------------