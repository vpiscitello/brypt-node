//------------------------------------------------------------------------------------------------
#include "CoordinatorState.hpp"
//------------------------------------------------------------------------------------------------

CCoordinatorState::CCoordinatorState()
    : m_mutex()
    , m_identifier()
    , m_technology(Endpoints::TechnologyType::Invalid)
{
}

//------------------------------------------------------------------------------------------------

CCoordinatorState::CCoordinatorState(
    Endpoints::TechnologyType technology,
    NetworkUtils::AddressComponentPair const& components)
    : m_mutex()
    , m_identifier()
    , m_entry()
    , m_technology(technology)
{
    m_entry = components.first
              + NetworkUtils::ComponentSeperator.data()
              + components.second;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer CCoordinatorState::GetIdentifier() const 
{
    std::shared_lock lock(m_mutex);
    return m_identifier;
}

//------------------------------------------------------------------------------------------------

std::string CCoordinatorState::GetEntry() const
{
    std::shared_lock lock(m_mutex);
    return m_entry;
}

//------------------------------------------------------------------------------------------------

Endpoints::TechnologyType CCoordinatorState::GetTechnology() const
{
    std::shared_lock lock(m_mutex);
    return m_technology;
}

//------------------------------------------------------------------------------------------------

void CCoordinatorState::SetIdentifier(BryptIdentifier::CContainer const& identifier)
{
    std::unique_lock lock(m_mutex);
    m_identifier = identifier;
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

void CCoordinatorState::SetTechnology(Endpoints::TechnologyType technology)
{
    std::unique_lock lock(m_mutex);
    m_technology = technology;
}

//------------------------------------------------------------------------------------------------