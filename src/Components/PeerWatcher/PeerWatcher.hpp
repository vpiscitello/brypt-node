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

class CEndpointManager;
class CMessage;
class CState;

//------------------------------------------------------------------------------------------------

class CPeerWatcher {
public:
    explicit CPeerWatcher(std::weak_ptr<CEndpointManager> const& wpEndpointManager);
    ~CPeerWatcher();

    bool Startup();
    bool Shutdown();

private:
    void Watch();
    void Heartbeat();

    std::weak_ptr<CEndpointManager> m_wpEndpointManager;

    TimeUtils::Timepoint m_lastCheckTimepoint;
    TimeUtils::Timepoint m_requiredUpdateTimepoint;

    bool m_process;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_worker;  
};

//------------------------------------------------------------------------------------------------