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
    NodeUtils::Timepoint GetRegisteredTimepoint() const;
    NodeUtils::Timepoint GetUpdatedTimepoint() const;

    void PushPeerName(NodeUtils::NodeIdType const& peerName);
    void RemovePeerName(NodeUtils::NodeIdType const& peerName);
    void SetRegisteredTimepoint(NodeUtils::Timepoint const& timepoint);
    void Updated();

private:
    mutable std::shared_mutex m_mutex;

    std::set<NodeUtils::NodeIdType> m_peerNames;

    NodeUtils::TimePeriod m_uptime; // The amount of time the node has been live
    NodeUtils::Timepoint m_registered; // The timestamp the node was added to the network
    NodeUtils::Timepoint m_updated;    // The timestamp the node was last updated
};

//------------------------------------------------------------------------------------------------