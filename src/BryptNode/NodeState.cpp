//------------------------------------------------------------------------------------------------
#include "NodeState.hpp"
//------------------------------------------------------------------------------------------------

CNodeState::CNodeState(BryptIdentifier::SharedContainer const& spBryptIdentifier)
    : m_mutex()
    , m_spBryptIdentifier(spBryptIdentifier)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_technologies()
{
}

//------------------------------------------------------------------------------------------------

CNodeState::CNodeState(
    BryptIdentifier::SharedContainer const& spBryptIdentifier,
    Endpoints::TechnologySet const& technologies)
    : m_mutex()
    , m_spBryptIdentifier(spBryptIdentifier)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_technologies(technologies)
{
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::SharedContainer CNodeState::GetBryptIdentifier() const
{
    std::shared_lock lock(m_mutex);
    return m_spBryptIdentifier;
}

//------------------------------------------------------------------------------------------------

NodeUtils::ClusterIdType CNodeState::GetCluster() const
{
    std::shared_lock lock(m_mutex);
    return m_cluster;
}

//------------------------------------------------------------------------------------------------

NodeUtils::DeviceOperation CNodeState::GetOperation() const
{
    std::shared_lock lock(m_mutex);
    return m_operation;
}

//------------------------------------------------------------------------------------------------

void CNodeState::SetBryptIdentifier(BryptIdentifier::SharedContainer const& spBryptIdentifier)
{
    std::unique_lock lock(m_mutex);
    m_spBryptIdentifier = spBryptIdentifier;
}

//------------------------------------------------------------------------------------------------

void CNodeState::SetCluster(NodeUtils::ClusterIdType const& cluster)
{
    std::unique_lock lock(m_mutex);
    m_cluster = cluster;
}

//------------------------------------------------------------------------------------------------

void CNodeState::SetOperation(NodeUtils::DeviceOperation operation)
{
    std::unique_lock lock(m_mutex);
    m_operation = operation;
}

//------------------------------------------------------------------------------------------------
