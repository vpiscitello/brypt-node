//----------------------------------------------------------------------------------------------------------------------
// File: NodeState.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/NodeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <string>
#include <cstdint>
#include <set>
#include <shared_mutex>
//----------------------------------------------------------------------------------------------------------------------

class NodeState {
public:
    explicit NodeState(Node::SharedIdentifier const& spNodeIdentifier);
    NodeState(
        Node::SharedIdentifier const& spNodeIdentifier,
        Network::ProtocolSet const& protocols);

    Node::SharedIdentifier const& GetNodeIdentifier() const;
    NodeUtils::ClusterIdType GetCluster() const;
    NodeUtils::DeviceOperation GetOperation() const;

    void SetNodeIdentifier(Node::SharedIdentifier const& spNodeIdentifier);
    void SetCluster(NodeUtils::ClusterIdType const& cluster);
    void SetOperation(NodeUtils::DeviceOperation operation);

private:
    mutable std::shared_mutex m_mutex;

    Node::SharedIdentifier m_spNodeIdentifier; // The unique identifier for this node

    NodeUtils::ClusterIdType m_cluster;   // Cluster identification number of the node's cluster
    NodeUtils::DeviceOperation m_operation;  // A enumeration value of the node's root status
    Network::ProtocolSet m_protocols; // Communication protocols of the node
};

//----------------------------------------------------------------------------------------------------------------------