//------------------------------------------------------------------------------------------------
#include "NetworkState.hpp"
//------------------------------------------------------------------------------------------------

CNetworkState::CNetworkState()
    : m_mutex()
    , m_uptime()
    , m_registered()
    , m_updated()
{
}

//------------------------------------------------------------------------------------------------

TimeUtils::TimePeriod CNetworkState::GetUptimeCount() const
{
    std::shared_lock lock(m_mutex);
    return m_uptime;
}

//------------------------------------------------------------------------------------------------

TimeUtils::Timepoint CNetworkState::GetRegisteredTimepoint() const
{
    std::shared_lock lock(m_mutex);
    return m_registered;
}

//------------------------------------------------------------------------------------------------

TimeUtils::Timepoint CNetworkState::GetUpdatedTimepoint() const
{
    std::shared_lock lock(m_mutex);
    return m_updated;
}

//------------------------------------------------------------------------------------------------

void CNetworkState::SetRegisteredTimepoint(TimeUtils::Timepoint const& timepoint)
{
    std::unique_lock lock(m_mutex);
    m_registered = timepoint;
}

//------------------------------------------------------------------------------------------------

void CNetworkState::Updated()
{
    std::unique_lock lock(m_mutex);
    m_updated = TimeUtils::GetSystemTimepoint();
}

//------------------------------------------------------------------------------------------------