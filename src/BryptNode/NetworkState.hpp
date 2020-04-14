//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
#include "../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <set>
#include <cstddef>
#include <shared_mutex>
#include <string>
//------------------------------------------------------------------------------------------------

class CNetworkState {
public:
    CNetworkState();

    std::set<NodeUtils::NodeIdType> GetPeerNames() const;
    std::size_t GetKnownNodes() const;
    TimeUtils::TimePeriod GetUptimeCount() const;
    TimeUtils::Timepoint GetRegisteredTimepoint() const;
    TimeUtils::Timepoint GetUpdatedTimepoint() const;

    void PushPeerName(NodeUtils::NodeIdType const& peerName);
    void RemovePeerName(NodeUtils::NodeIdType const& peerName);
    void SetRegisteredTimepoint(TimeUtils::Timepoint const& timepoint);
    void Updated();

private:
    mutable std::shared_mutex m_mutex;

    std::set<NodeUtils::NodeIdType> m_peerNames;

    TimeUtils::TimePeriod m_uptime; // The amount of time the node has been live
    TimeUtils::Timepoint m_registered; // The timestamp the node was added to the network
    TimeUtils::Timepoint m_updated;    // The timestamp the node was last updated
};

//------------------------------------------------------------------------------------------------