//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <cstdint>
#include <set>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class NodeState {
public:
    explicit NodeState(BryptIdentifier::SharedContainer const& spBryptIdentifier);
    NodeState(
        BryptIdentifier::SharedContainer const& spBryptIdentifier,
        Endpoints::TechnologySet const& technologies);

    BryptIdentifier::SharedContainer GetBryptIdentifier() const;
    NodeUtils::ClusterIdType GetCluster() const;
    NodeUtils::DeviceOperation GetOperation() const;

    void SetBryptIdentifier(BryptIdentifier::SharedContainer const& spBryptIdentifier);
    void SetCluster(NodeUtils::ClusterIdType const& cluster);
    void SetOperation(NodeUtils::DeviceOperation operation);

private:
    mutable std::shared_mutex m_mutex;

    BryptIdentifier::SharedContainer m_spBryptIdentifier; // BryptIdentifier of the node

    NodeUtils::ClusterIdType m_cluster;   // Cluster identification number of the node's cluster
    NodeUtils::DeviceOperation m_operation;  // A enumeration value of the node's root status
    Endpoints::TechnologySet m_technologies; // Communication technologies of the node
};

//------------------------------------------------------------------------------------------------