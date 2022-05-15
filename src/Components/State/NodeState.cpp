//----------------------------------------------------------------------------------------------------------------------
#include "NodeState.hpp"
//----------------------------------------------------------------------------------------------------------------------

NodeState::NodeState(Node::SharedIdentifier const& spNodeIdentifier)
    : m_mutex()
    , m_spNodeIdentifier(spNodeIdentifier)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::Leaf)
    , m_protocols()
{
}

//----------------------------------------------------------------------------------------------------------------------

NodeState::NodeState(
    Node::SharedIdentifier const& spNodeIdentifier,
    Network::ProtocolSet const& protocols)
    : m_mutex()
    , m_spNodeIdentifier(spNodeIdentifier)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::Leaf)
    , m_protocols(protocols)
{
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& NodeState::GetNodeIdentifier() const
{
    std::shared_lock lock(m_mutex);
    return m_spNodeIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

NodeUtils::ClusterIdType NodeState::GetCluster() const
{
    std::shared_lock lock(m_mutex);
    return m_cluster;
}

//----------------------------------------------------------------------------------------------------------------------

NodeUtils::DeviceOperation NodeState::GetOperation() const
{
    std::shared_lock lock(m_mutex);
    return m_operation;
}

//----------------------------------------------------------------------------------------------------------------------

void NodeState::SetNodeIdentifier(Node::SharedIdentifier const& spNodeIdentifier)
{
    std::unique_lock lock(m_mutex);
    m_spNodeIdentifier = spNodeIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

void NodeState::SetCluster(NodeUtils::ClusterIdType const& cluster)
{
    std::unique_lock lock(m_mutex);
    m_cluster = cluster;
}

//----------------------------------------------------------------------------------------------------------------------

void NodeState::SetOperation(NodeUtils::DeviceOperation operation)
{
    std::unique_lock lock(m_mutex);
    m_operation = operation;
}

//----------------------------------------------------------------------------------------------------------------------
