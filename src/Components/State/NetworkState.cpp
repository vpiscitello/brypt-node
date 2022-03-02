//----------------------------------------------------------------------------------------------------------------------
#include "NetworkState.hpp"
//----------------------------------------------------------------------------------------------------------------------

NetworkState::NetworkState()
    : m_mutex()
    , m_uptime()
    , m_registered()
    , m_updated()
{
}

//----------------------------------------------------------------------------------------------------------------------

TimeUtils::Timestamp NetworkState::GetUptimeCount() const
{
    std::shared_lock lock(m_mutex);
    return m_uptime;
}

//----------------------------------------------------------------------------------------------------------------------

TimeUtils::Timepoint NetworkState::GetRegisteredTimepoint() const
{
    std::shared_lock lock(m_mutex);
    return m_registered;
}

//----------------------------------------------------------------------------------------------------------------------

TimeUtils::Timepoint NetworkState::GetUpdatedTimepoint() const
{
    std::shared_lock lock(m_mutex);
    return m_updated;
}

//----------------------------------------------------------------------------------------------------------------------

void NetworkState::SetRegisteredTimepoint(TimeUtils::Timepoint const& timepoint)
{
    std::unique_lock lock(m_mutex);
    m_registered = timepoint;
}

//----------------------------------------------------------------------------------------------------------------------

void NetworkState::Updated()
{
    std::unique_lock lock(m_mutex);
    m_updated = TimeUtils::GetSystemTimepoint();
}

//----------------------------------------------------------------------------------------------------------------------