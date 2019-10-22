//------------------------------------------------------------------------------------------------
// File: PeerWatcher.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "PeerWatcher.hpp"
//------------------------------------------------------------------------------------------------
#include "../Connection/Connection.hpp"
#include "../../State.hpp"
#include "../../Utilities/Message.hpp"
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

CPeerWatcher::CPeerWatcher(
    std::weak_ptr<CState> const& state,
    std::weak_ptr<NodeUtils::ConnectionMap> const& peers)
    : m_state(state)
    , m_watched(peers)
    , m_lastCheckTimePoint()
    , m_requiredUpdateTimePoint()
    , m_process(true)
    , m_mutex()
    , m_cv()
    , m_worker()
{
    m_worker = std::thread(&CPeerWatcher::watch, this);
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
        m_worker = std::thread(&CPeerWatcher::watch, this);
    }
    return m_worker.joinable();
}

//------------------------------------------------------------------------------------------------

bool CPeerWatcher::Shutdown()
{
    // Stop the worker thread from processing the connections
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_process = false;
    }

    m_cv.notify_all();
    
    if (m_worker.joinable()) {
        m_worker.join();
    }
    
    return !m_worker.joinable();
}

//-------------------------------------------------------------------------------;-----------------

void CPeerWatcher::watch()
{
    do {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto const stop = std::chrono::system_clock::now() + local::timeout;
            auto const terminate = m_cv.wait_until(lock, stop, [&]{ return !m_process; });
            // Terminate thread if the timeout was not reached
            if (terminate) {
                return;
            }
        }

        if (auto const spWatched = m_watched.lock()) {
            for (auto itr = spWatched->begin() ; itr != spWatched->end(); ++itr) {
                NodeUtils::TimePoint updateTimePoint = itr->second->GetUpdateClock();
                if (updateTimePoint < m_requiredUpdateTimePoint) {
                    NodeUtils::printo("Peer " + itr->first + " needs to be checked with a heartbeat", NodeUtils::PrintType::WATCHER);
                    // TODO: Mark a node for a heartbeat
                }
            }
        }

        m_requiredUpdateTimePoint = NodeUtils::GetSystemTimePoint();
    } while(true);
}

void CPeerWatcher::heartbeat()
{

}
