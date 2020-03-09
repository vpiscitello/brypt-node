//------------------------------------------------------------------------------------------------
#include "NodeState.hpp"
//------------------------------------------------------------------------------------------------

CNodeState::CNodeState()
    : m_mutex()
    , m_id(0) // TODO: Set Machine UUID for state
    , m_serial()
    , m_address()
    , m_port()
    , m_binding()
    , m_nextAvailablePort(0)
    , m_cluster(0)
    , m_operation(NodeUtils::DeviceOperation::None)
    , m_technologies()
{
}

//------------------------------------------------------------------------------------------------

CNodeState::CNodeState(
    std::string_view const& interface,
    NodeUtils::AddressComponentPair const& bindingComponents,
    NodeUtils::DeviceOperation operation,
    std::set<NodeUtils::TechnologyType> const& technologies)
    : m_mutex()
    , m_id(0) // TODO: Set Machine UUID for state
    , m_serial()
    , m_address()
    , m_port(bindingComponents.second)
    , m_binding()
    , m_publisherPort()
    , m_nextAvailablePort(0)
    , m_cluster(0)
    , m_operation(operation)
    , m_technologies(technologies)
{
    // Determine the actual IP address of the node based on the interface 
    // provided. It is currently assumed, the address provided in the binding
    // components is a wildcard character that needs expanding.
    m_address = NodeUtils::GetLocalAddress(interface);

    // Construct a full binding ip:port from the address 
    m_binding = m_address + ":" + m_port;

    try {
        std::int32_t const portNumber = std::stoi(bindingComponents.second);
        m_publisherPort = std::to_string(portNumber + 1);
        m_nextAvailablePort = portNumber + NodeUtils::PORT_GAP;
    } catch (...) {
        // port must not have been a valid integer...
    }
}

//------------------------------------------------------------------------------------------------

std::uint32_t CNodeState::GetNextPort()
{
    std::shared_lock lock(m_mutex);
    return ++m_nextAvailablePort; // TODO: Need smarter method of choosing the next port
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

std::string CNodeState::GetBinding() const
{
    std::shared_lock lock(m_mutex);
    return m_binding;
}

//------------------------------------------------------------------------------------------------

NodeUtils::NetworkAddress CNodeState::GetAddress() const
{
    std::shared_lock lock(m_mutex);
    return m_address;
}

//------------------------------------------------------------------------------------------------

NodeUtils::PortNumber CNodeState::GetPort() const
{
    std::shared_lock lock(m_mutex);
    return m_port;
}

//------------------------------------------------------------------------------------------------

NodeUtils::PortNumber CNodeState::GetPublisherPort() const
{
    std::shared_lock lock(m_mutex);
    return m_publisherPort;
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