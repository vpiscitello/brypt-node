//----------------------------------------------------------------------------------------------------------------------
#include "NodeState.hpp"
//----------------------------------------------------------------------------------------------------------------------

NodeState::NodeState(BryptIdentifier::SharedContainer const& spBryptIdentifier)
    : m_mutex()
    , m_spBryptIdentifier(spBryptIdentifier)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_protocols()
{
}

//----------------------------------------------------------------------------------------------------------------------

NodeState::NodeState(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    Network::ProtocolSet const& protocols)
    : m_mutex()
    , m_spBryptIdentifier(spBryptIdentifier)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_protocols(protocols)
{
}

//----------------------------------------------------------------------------------------------------------------------

BryptIdentifier::SharedContainer NodeState::GetBryptIdentifier() const
{
    std::shared_lock lock(m_mutex);
    return m_spBryptIdentifier;
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

void NodeState::SetBryptIdentifier(BryptIdentifier::SharedContainer const& spBryptIdentifier)
{
    std::unique_lock lock(m_mutex);
    m_spBryptIdentifier = spBryptIdentifier;
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
