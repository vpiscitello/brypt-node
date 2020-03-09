//------------------------------------------------------------------------------------------------
#include "CoordinatorState.hpp"
//------------------------------------------------------------------------------------------------

CCoordinatorState::CCoordinatorState()
    : m_mutex()
    , m_id(0)
    , m_address()
    , m_requestPort()
    , m_publisherPort()
    , m_technology(NodeUtils::TechnologyType::None)
{
}

//------------------------------------------------------------------------------------------------

CCoordinatorState::CCoordinatorState(
    NodeUtils::TechnologyType technology,
    NodeUtils::AddressComponentPair entryComponents)
    : m_mutex()
    , m_id(0)
    , m_address(entryComponents.first)
    , m_requestPort(entryComponents.second)
    , m_publisherPort()
    , m_requestEntry()
    , m_publisherEntry()
    , m_technology(technology)
{
    // If the address or port is not set on the entry components it implies 
    // that a coordinator has not been set in the configuration and thus there
    // is no initial state to parse. 
    if (!m_address.empty() && !m_requestPort.empty()) {
        try {
            std::int32_t const port = std::stoi(m_requestPort);
            m_publisherPort = std::to_string(port + 1);

            m_requestEntry = m_address + NodeUtils::ADDRESS_COMPONENT_SEPERATOR + m_requestPort;
            m_publisherEntry = m_address + NodeUtils::ADDRESS_COMPONENT_SEPERATOR + m_publisherPort;
        } catch (...) {
            // request port must not have been a valid integer...
        }
    }
}

//------------------------------------------------------------------------------------------------

NodeUtils::NodeIdType CCoordinatorState::GetId() const 
{
    std::shared_lock lock(m_mutex);
    return m_id;
}

//------------------------------------------------------------------------------------------------

NodeUtils::NetworkAddress CCoordinatorState::GetAddress() const
{
    std::shared_lock lock(m_mutex);
    return m_address;
}

//------------------------------------------------------------------------------------------------

std::string CCoordinatorState::GetRequestEntry() const
{
    std::shared_lock lock(m_mutex);
    return m_requestEntry;
}

//------------------------------------------------------------------------------------------------

NodeUtils::PortNumber CCoordinatorState::GetRequestPort() const
{
    std::shared_lock lock(m_mutex);
    return m_requestPort;
}

//------------------------------------------------------------------------------------------------

std::string CCoordinatorState::GetPublisherEntry() const
{
    std::shared_lock lock(m_mutex);
    return m_publisherEntry;
}

//------------------------------------------------------------------------------------------------

NodeUtils::PortNumber CCoordinatorState::GetPublisherPort() const
{
    std::shared_lock lock(m_mutex);
    return m_publisherPort;
}

//------------------------------------------------------------------------------------------------

NodeUtils::TechnologyType CCoordinatorState::GetTechnology() const
{
    std::shared_lock lock(m_mutex);
    return m_technology;
}

//------------------------------------------------------------------------------------------------

void CCoordinatorState::SetId(NodeUtils::NodeIdType const& id)
{
    std::unique_lock lock(m_mutex);
    m_id = id;
}

//------------------------------------------------------------------------------------------------

void CCoordinatorState::SetAddress(NodeUtils::NetworkAddress const& address)
{
    std::unique_lock lock(m_mutex);
    m_address = address;
}

//------------------------------------------------------------------------------------------------

void CCoordinatorState::SetRequestPort(NodeUtils::PortNumber const& port)
{
    std::unique_lock lock(m_mutex);
    m_requestPort = port;
}

//------------------------------------------------------------------------------------------------

void CCoordinatorState::SetPublisherPort(NodeUtils::PortNumber const& port)
{
    std::unique_lock lock(m_mutex);
    m_publisherPort = port;
}

//------------------------------------------------------------------------------------------------

void CCoordinatorState::SetTechnology(NodeUtils::TechnologyType technology)
{
    std::unique_lock lock(m_mutex);
    m_technology = technology;
}

//------------------------------------------------------------------------------------------------