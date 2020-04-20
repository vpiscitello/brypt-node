//------------------------------------------------------------------------------------------------
#include "NodeState.hpp"
//------------------------------------------------------------------------------------------------

CNodeState::CNodeState()
    : m_mutex()
    , m_id(0) // TODO: Set Machine UUID for state
    , m_serial()
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_technologies()
{
}

//------------------------------------------------------------------------------------------------

CNodeState::CNodeState(
    NodeUtils::DeviceOperation operation,
    std::set<NodeUtils::TechnologyType> const& technologies)
    : m_mutex()
    , m_id(0) // TODO: Set Machine UUID for state
    , m_serial()
    , m_cluster(0)
    , m_operation(operation)
    , m_technologies(technologies)
{
}

//------------------------------------------------------------------------------------------------

NodeUtils::NodeIdType CNodeState::GetId() const
{
    std::shared_lock lock(m_mutex);
    return m_id;
}

//------------------------------------------------------------------------------------------------

NodeUtils::SerialNumber CNodeState::GetSerial() const
{
    std::shared_lock lock(m_mutex);
    return m_serial;
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

std::set<NodeUtils::TechnologyType> CNodeState::GetTechnologies() const
{
    std::shared_lock lock(m_mutex);
    return m_technologies;
}

//------------------------------------------------------------------------------------------------

void CNodeState::SetId(NodeUtils::NodeIdType const& id)
{
    std::unique_lock lock(m_mutex);
    m_id = id;
}

//------------------------------------------------------------------------------------------------

void CNodeState::SetSerial(NodeUtils::SerialNumber const& serial)
{
    std::unique_lock lock(m_mutex);
    m_serial = serial;
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

void CNodeState::SetTechnologies(std::set<NodeUtils::TechnologyType> const& technologies)
{
    std::unique_lock lock(m_mutex);
    m_technologies = technologies;
}

//------------------------------------------------------------------------------------------------