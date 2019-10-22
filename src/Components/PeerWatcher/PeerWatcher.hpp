//------------------------------------------------------------------------------------------------
// File: PeerWatcher.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
class CConnection;
class CMessage;
class CState;
//------------------------------------------------------------------------------------------------

class CPeerWatcher {
    public:
        CPeerWatcher(
            std::weak_ptr<CState> const& state,
            std::weak_ptr<NodeUtils::ConnectionMap> const& peers)
            : m_state(state)
            , m_watched(peers)
            , m_lastCheckTimePoint()
            , m_requiredUpdateTimePoint()
            , m_process(true)
            , m_active(false)
            , m_worker(std::thread(&CPeerWatcher::watch, this))
            , m_mutex()
            , m_terminate()
        {
        };

        ~CPeerWatcher()
        {
            // Stop the worker thread from processing the connections
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_process = false;
            }
            m_terminate.notify_all();
            // NOTE: GDB in VSCode - WSL seems to be crashes when m_worker.join() is used. The thread will successfully 
            // complete/return, however, GDB will hang in futex_wait. m_worker.join() does not fail in normal run.
            // Occasionally m_worker.join() will work, but that is the exception.
            if (m_worker.joinable()) {
                m_worker.join();
            }
        };

    private:
        void watch();
        void heartbeat();

        std::weak_ptr<CState> m_state;
        std::weak_ptr<NodeUtils::ConnectionMap> m_watched;

        NodeUtils::TimePoint m_lastCheckTimePoint;
        NodeUtils::TimePoint m_requiredUpdateTimePoint;

        bool m_process;

        bool m_active;
        std::thread m_worker;
        std::mutex m_mutex;
        std::condition_variable m_terminate;

};

//------------------------------------------------------------------------------------------------