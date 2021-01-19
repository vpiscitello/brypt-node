//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <set>
#include <shared_mutex>
#include <string>
//------------------------------------------------------------------------------------------------

class NetworkState {
public:
    NetworkState();

    TimeUtils::Timestamp GetUptimeCount() const;
    TimeUtils::Timepoint GetRegisteredTimepoint() const;
    TimeUtils::Timepoint GetUpdatedTimepoint() const;

    void SetRegisteredTimepoint(TimeUtils::Timepoint const& timepoint);
    void Updated();

private:
    mutable std::shared_mutex m_mutex;

    TimeUtils::Timestamp m_uptime; // The amount of time the node has been live
    TimeUtils::Timepoint m_registered; // The timestamp the node was added to the network
    TimeUtils::Timepoint m_updated;    // The timestamp the node was last updated
};

//------------------------------------------------------------------------------------------------