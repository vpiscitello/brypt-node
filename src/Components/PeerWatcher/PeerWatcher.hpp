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
        std::weak_ptr<NodeUtils::ConnectionMap> const& peers);
    ~CPeerWatcher();

    bool Startup();
    bool Shutdown();

private:
    void watch();
    void heartbeat();

    std::weak_ptr<CState> m_state;
    std::weak_ptr<NodeUtils::ConnectionMap> m_watched;

    NodeUtils::TimePoint m_lastCheckTimePoint;
    NodeUtils::TimePoint m_requiredUpdateTimePoint;

    bool m_process;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;
    
};

//------------------------------------------------------------------------------------------------