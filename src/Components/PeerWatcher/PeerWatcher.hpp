//------------------------------------------------------------------------------------------------
// File: PeerWatcher.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Endpoints/EndpointTypes.hpp"
#include "../../Utilities/NodeUtils.hpp"
#include "../../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <condition_variable>
#include <mutex>
#include <thread>
//------------------------------------------------------------------------------------------------

class CEndpoint;
class CMessage;
class CState;

//------------------------------------------------------------------------------------------------

class CPeerWatcher {
public:
    explicit CPeerWatcher(std::weak_ptr<EndpointMap> const& wpPeers);
    ~CPeerWatcher();

    bool Startup();
    bool Shutdown();

private:
    void watch();
    void heartbeat();

    std::weak_ptr<EndpointMap> m_wpWatched;

    TimeUtils::Timepoint m_lastCheckTimepoint;
    TimeUtils::Timepoint m_requiredUpdateTimepoint;

    bool m_process;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;  
};

//------------------------------------------------------------------------------------------------