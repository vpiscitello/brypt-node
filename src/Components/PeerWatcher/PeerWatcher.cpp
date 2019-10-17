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
constexpr std::chrono::milliseconds timeout = std::chrono::seconds(10);
//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
} // namespace
//------------------------------------------------------------------------------------------------

void CPeerWatcher::watch()
{
    m_active = true;

    do {
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

        std::this_thread::sleep_for(local::timeout);
    } while(true);
}

void CPeerWatcher::heartbeat()
{

}
