//------------------------------------------------------------------------------------------------
// File: PeerWatcher.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "PeerWatcher.hpp"
//------------------------------------------------------------------------------------------------
#include "../Endpoints/EndpointManager.hpp"
#include "../../Message/Message.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------------------------------
namespace local {
//------------------------------------------------------------------------------------------------

constexpr std::chrono::seconds timeout = std::chrono::seconds(10);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

CPeerWatcher::CPeerWatcher(std::weak_ptr<CEndpointManager> const& wpEndpointManager)
    : m_wpEndpointManager(wpEndpointManager)
    , m_lastCheckTimepoint()
    , m_requiredUpdateTimepoint()
    , m_process(true)
    , m_mutex()
    , m_cv()
    , m_worker()
{
}

//------------------------------------------------------------------------------------------------

CPeerWatcher::~CPeerWatcher()
{
    if (m_worker.joinable()) {
        Shutdown();
    }
}

//------------------------------------------------------------------------------------------------

bool CPeerWatcher::Startup()
{
    if (!m_worker.joinable()) {
        m_worker = std::thread(&CPeerWatcher::Watch, this);
    }
    return m_worker.joinable();
}

//------------------------------------------------------------------------------------------------

bool CPeerWatcher::Shutdown()
{
    {
        std::scoped_lock lock(m_mutex);
        m_process = false;
    }

    m_cv.notify_all();
    
    if (m_worker.joinable()) {
        m_worker.join();
    }
    
    return !m_worker.joinable();
}

//-------------------------------------------------------------------------------;-----------------

void CPeerWatcher::Watch()
{
    do {
        {
            std::unique_lock lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + local::timeout;
            auto const terminate = m_cv.wait_until(lock, stop, [&]{ return !m_process; });
            // Terminate thread if the timeout was not reached
            if (terminate) {
                return;
            }
        }

        /*
        if (auto const spWatched = m_watched.lock()) {
            for (auto itr = spWatched->begin() ; itr != spWatched->end(); ++itr) {
                TimeUtils::Timepoint updateTimepoint = itr->second->GetUpdateClock();
                if (updateTimepoint < m_requiredUpdateTimepoint) {
                    NodeUtils::printo("Peer " + std::to_string(itr->first) + " needs to be checked with a heartbeat", NodeUtils::PrintType::PeerWatcher);
                    // TODO: Mark a node for a heartbeat
                }
            }
        }
        */

        m_requiredUpdateTimepoint = TimeUtils::GetSystemTimepoint();
    } while(true);
}

//------------------------------------------------------------------------------------------------

void CPeerWatcher::Heartbeat()
{

}

//------------------------------------------------------------------------------------------------