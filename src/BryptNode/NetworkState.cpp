//------------------------------------------------------------------------------------------------
#include "NetworkState.hpp"
//------------------------------------------------------------------------------------------------

CNetworkState::CNetworkState()
    : m_mutex()
    , m_peerNames()
    , m_uptime()
    , m_registered()
    , m_updated()
{
}

//------------------------------------------------------------------------------------------------

std::set<NodeUtils::NodeIdType> CNetworkState::GetPeerNames() const 
{
    std::shared_lock lock(m_mutex);
    return m_peerNames;
}

//------------------------------------------------------------------------------------------------

std::size_t CNetworkState::GetKnownNodes() const
{
    std::shared_lock lock(m_mutex);
    return m_peerNames.size();
}

//------------------------------------------------------------------------------------------------

NodeUtils::TimePeriod CNetworkState::GetUptimeCount() const
{
    std::shared_lock lock(m_mutex);
    return m_uptime;
}

//------------------------------------------------------------------------------------------------

NodeUtils::Timepoint CNetworkState::GetRegisteredTimepoint() const
{
    std::shared_lock lock(m_mutex);
    return m_registered;
}

//------------------------------------------------------------------------------------------------

NodeUtils::Timepoint CNetworkState::GetUpdatedTimepoint() const
{
    std::shared_lock lock(m_mutex);
    return m_updated;
}

//------------------------------------------------------------------------------------------------

void CNetworkState::PushPeerName(NodeUtils::NodeIdType const& peerName)
{
    std::unique_lock lock(m_mutex);
    m_peerNames.emplace(peerName);
    m_updated = NodeUtils::GetSystemTimepoint();
}

//------------------------------------------------------------------------------------------------

void CNetworkState::RemovePeerName(NodeUtils::NodeIdType const& peerName)
{
    std::unique_lock lock(m_mutex);
    std::size_t const eraseCount = m_peerNames.erase(peerName);

    // only update if we actually erased the peer.
    if (eraseCount > 0) {
      m_updated = NodeUtils::GetSystemTimepoint();
    }
}

//------------------------------------------------------------------------------------------------

void CNetworkState::SetRegisteredTimepoint(NodeUtils::Timepoint const& timepoint)
{
    std::unique_lock lock(m_mutex);
    m_registered = timepoint;
}

//------------------------------------------------------------------------------------------------

void CNetworkState::Updated()
{
    std::unique_lock lock(m_mutex);
    m_updated = NodeUtils::GetSystemTimepoint();
}

//------------------------------------------------------------------------------------------------