//------------------------------------------------------------------------------------------------
#include "CoordinatorState.hpp"
//------------------------------------------------------------------------------------------------

CCoordinatorState::CCoordinatorState()
    : m_mutex()
    , m_spBryptIdentifier()
{
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::SharedContainer CCoordinatorState::GetBryptIdentifier() const 
{
    std::shared_lock lock(m_mutex);
    return m_spBryptIdentifier;
}

//------------------------------------------------------------------------------------------------

void CCoordinatorState::SetBryptIdentifier(BryptIdentifier::SharedContainer const& spBryptIdentifier)
{
    std::unique_lock lock(m_mutex);
    m_spBryptIdentifier = spBryptIdentifier;
}

//------------------------------------------------------------------------------------------------