//------------------------------------------------------------------------------------------------
#include "SecurityState.hpp"
//------------------------------------------------------------------------------------------------

CSecurityState::CSecurityState()
    : m_mutex()
    , m_standard()
{
}

//------------------------------------------------------------------------------------------------

CSecurityState::CSecurityState(std::string_view const& protocol)
    : m_mutex()
    , m_standard(protocol)
{
}

//------------------------------------------------------------------------------------------------

std::string CSecurityState::GetProtocol() const
{
    std::shared_lock lock(m_mutex);
    return m_standard;
}

//------------------------------------------------------------------------------------------------

void CSecurityState::SetProtocol(std::string const& protocol)
{
    std::unique_lock lock(m_mutex);
    m_standard = protocol;
}

//------------------------------------------------------------------------------------------------