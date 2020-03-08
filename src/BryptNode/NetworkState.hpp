//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
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
    NodeUtils::TimePeriod GetUptimeCount() const;
    NodeUtils::TimePoint GetRegisteredTimePoint() const;
    NodeUtils::TimePoint GetUpdatedTimePoint() const;

    void PushPeerName(NodeUtils::NodeIdType const& peerName);
    void RemovePeerName(NodeUtils::NodeIdType const& peerName);
    void SetRegisteredTimePoint(NodeUtils::TimePoint const& timepoint);
    void Updated();

private:
    mutable std::shared_mutex m_mutex;

    std::set<NodeUtils::NodeIdType> m_peerNames;

    NodeUtils::TimePeriod m_uptime; // The amount of time the node has been live
    NodeUtils::TimePoint m_registered; // The timestamp the node was added to the network
    NodeUtils::TimePoint m_updated;    // The timestamp the node was last updated
};

//------------------------------------------------------------------------------------------------