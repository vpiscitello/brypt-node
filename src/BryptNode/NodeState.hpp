//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <cstdint>
#include <set>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CNodeState {
public:
    CNodeState();

    CNodeState(Endpoints::TechnologySet const& technologies);

    NodeUtils::NodeIdType GetId() const;
    NodeUtils::SerialNumber GetSerial() const;
    NodeUtils::ClusterIdType GetCluster() const;
    NodeUtils::DeviceOperation GetOperation() const;
    std::set<Endpoints::TechnologyType> GetTechnologies() const;

    void SetId(NodeUtils::NodeIdType const& id);
    void SetSerial(NodeUtils::SerialNumber const& serial);
    void SetCluster(NodeUtils::ClusterIdType const& cluster);
    void SetOperation(NodeUtils::DeviceOperation operation);
    void SetTechnologies(std::set<Endpoints::TechnologyType> const& technologies);

private:
    mutable std::shared_mutex m_mutex;

    NodeUtils::NodeIdType m_id;// Network identification number of the node
    NodeUtils::SerialNumber m_serial; // Hardset identification number of the device

    NodeUtils::ClusterIdType m_cluster;   // Cluster identification number of the node's cluster
    NodeUtils::DeviceOperation m_operation;  // A enumeration value of the node's root status
    Endpoints::TechnologySet m_technologies; // Communication technologies of the node
};

//------------------------------------------------------------------------------------------------