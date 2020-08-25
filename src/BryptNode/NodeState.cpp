//------------------------------------------------------------------------------------------------
#include "NodeState.hpp"
//------------------------------------------------------------------------------------------------

CNodeState::CNodeState(BryptIdentifier::CContainer const& identifier)
    : m_mutex()
    , m_identifier(identifier)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_technologies()
{
}

//------------------------------------------------------------------------------------------------

CNodeState::CNodeState(
    BryptIdentifier::CContainer const& identifier,
    Endpoints::TechnologySet const& technologies)
    : m_mutex()
    , m_identifier(identifier)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_technologies(technologies)
{
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer CNodeState::GetIdentifier() const
{
    std::shared_lock lock(m_mutex);
    return m_identifier;
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

void CNodeState::SetIdentifier(BryptIdentifier::CContainer const& identifier)
{
    std::unique_lock lock(m_mutex);
    m_identifier = identifier;
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
