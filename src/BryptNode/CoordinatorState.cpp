//----------------------------------------------------------------------------------------------------------------------
#include "CoordinatorState.hpp"
//----------------------------------------------------------------------------------------------------------------------

CoordinatorState::CoordinatorState()
    : m_mutex()
    , m_spBryptIdentifier()
{
}

//----------------------------------------------------------------------------------------------------------------------

BryptIdentifier::SharedContainer CoordinatorState::GetBryptIdentifier() const 
{
    std::shared_lock lock(m_mutex);
    return m_spBryptIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

void CoordinatorState::SetBryptIdentifier(BryptIdentifier::SharedContainer const& spBryptIdentifier)
{
    std::unique_lock lock(m_mutex);
    m_spBryptIdentifier = spBryptIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------