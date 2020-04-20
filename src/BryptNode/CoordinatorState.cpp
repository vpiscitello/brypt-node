//------------------------------------------------------------------------------------------------
#include "CoordinatorState.hpp"
//------------------------------------------------------------------------------------------------

CCoordinatorState::CCoordinatorState()
    : m_mutex()
    , m_id(0)
    , m_technology(NodeUtils::TechnologyType::None)
{
}

//------------------------------------------------------------------------------------------------

CCoordinatorState::CCoordinatorState(
    NodeUtils::TechnologyType technology,
    NetworkUtils::AddressComponentPair const& components)
    : m_mutex()
    , m_id(0)
    , m_entry()
    , m_technology(technology)
{
    m_entry = components.first
              + NetworkUtils::ComponentSeperator.data()
              + components.second;
}

//------------------------------------------------------------------------------------------------

NodeUtils::NodeIdType CCoordinatorState::GetId() const 
{
    std::shared_lock lock(m_mutex);
    return m_id;
}

//------------------------------------------------------------------------------------------------

std::string CCoordinatorState::GetEntry() const
{
    std::shared_lock lock(m_mutex);
    return m_entry;
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

void CCoordinatorState::SetEntry(NetworkUtils::AddressComponentPair const& components)
{
    std::unique_lock lock(m_mutex);
    m_entry = components.first 
              + NetworkUtils::ComponentSeperator.data()
              + components.second;
}

//------------------------------------------------------------------------------------------------

void CCoordinatorState::SetTechnology(NodeUtils::TechnologyType technology)
{
    std::unique_lock lock(m_mutex);
    m_technology = technology;
}

//------------------------------------------------------------------------------------------------