//------------------------------------------------------------------------------------------------
// File: PeerWatcher.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
class CEndpoint;
class CMessage;
class CState;
//------------------------------------------------------------------------------------------------

class CPeerWatcher {
public:
    explicit CPeerWatcher(std::weak_ptr<NodeUtils::ConnectionMap> const& peers);
    ~CPeerWatcher();

    bool Startup();
    bool Shutdown();

private:
    void watch();
    void heartbeat();

    std::weak_ptr<NodeUtils::ConnectionMap> m_wpWatched;

    NodeUtils::Timepoint m_lastCheckTimepoint;
    NodeUtils::Timepoint m_requiredUpdateTimepoint;

    bool m_process;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;  
};

//------------------------------------------------------------------------------------------------