//----------------------------------------------------------------------------------------------------------------------
#include "CoordinatorState.hpp"
//----------------------------------------------------------------------------------------------------------------------

CoordinatorState::CoordinatorState()
    : m_mutex()
    , m_spNodeIdentifier()
{
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier CoordinatorState::GetNodeIdentifier() const 
{
    std::shared_lock lock(m_mutex);
    return m_spNodeIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

void CoordinatorState::SetNodeIdentifier(Node::SharedIdentifier const& spNodeIdentifier)
{
    std::unique_lock lock(m_mutex);
    m_spNodeIdentifier = spNodeIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------