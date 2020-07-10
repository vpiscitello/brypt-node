//------------------------------------------------------------------------------------------------
#include "NodeState.hpp"
//------------------------------------------------------------------------------------------------

CNodeState::CNodeState(NodeUtils::NodeIdType id)
    : m_mutex()
    , m_id(id)
    , m_serial()
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_technologies()
{
}

//------------------------------------------------------------------------------------------------

CNodeState::CNodeState(NodeUtils::NodeIdType id, Endpoints::TechnologySet const& technologies)
    : m_mutex()
    , m_id(id)
    , m_serial()
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
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

std::set<Endpoints::TechnologyType> CNodeState::GetTechnologies() const
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

void CNodeState::SetTechnologies(std::set<Endpoints::TechnologyType> const& technologies)
{
    std::unique_lock lock(m_mutex);
    m_technologies = technologies;
}

//------------------------------------------------------------------------------------------------