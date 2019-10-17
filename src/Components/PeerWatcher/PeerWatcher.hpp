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
            , m_worker(std::thread(&CPeerWatcher::watch, this))
            , m_active(false)
            , m_workerMutex()
            , m_workerConditional()
        {
        }

    private:
        void watch();
        void heartbeat();

        std::weak_ptr<CState> m_state;
        std::weak_ptr<NodeUtils::ConnectionMap> m_watched;

        NodeUtils::TimePoint m_lastCheckTimePoint;
        NodeUtils::TimePoint m_requiredUpdateTimePoint;

        std::thread m_worker;
        bool m_active;
        std::mutex m_workerMutex;
        std::condition_variable m_workerConditional;

};

//------------------------------------------------------------------------------------------------