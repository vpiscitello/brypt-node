//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../BryptIdentifier/BryptIdentifier.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <cstdint>
#include <set>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CNodeState {
public:
    explicit CNodeState(BryptIdentifier::CContainer const& identifier);
    CNodeState(
        BryptIdentifier::CContainer const& identifier,
        Endpoints::TechnologySet const& technologies);

    BryptIdentifier::CContainer GetIdentifier() const;
    NodeUtils::ClusterIdType GetCluster() const;
    NodeUtils::DeviceOperation GetOperation() const;

    void SetIdentifier(BryptIdentifier::CContainer const& identifier);
    void SetCluster(NodeUtils::ClusterIdType const& cluster);
    void SetOperation(NodeUtils::DeviceOperation operation);

private:
    mutable std::shared_mutex m_mutex;

    BryptIdentifier::CContainer m_identifier; // BryptIdentifier of the node

    NodeUtils::ClusterIdType m_cluster;   // Cluster identification number of the node's cluster
    NodeUtils::DeviceOperation m_operation;  // A enumeration value of the node's root status
    Endpoints::TechnologySet m_technologies; // Communication technologies of the node
};

//------------------------------------------------------------------------------------------------