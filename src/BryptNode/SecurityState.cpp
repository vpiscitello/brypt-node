//------------------------------------------------------------------------------------------------
#include "SecurityState.hpp"
//------------------------------------------------------------------------------------------------

CSecurityState::CSecurityState(Security::Strategy strategy, std::string_view authority)
    : m_mutex()
    , m_strategy(strategy)
    , m_authority(authority)
    , m_token()
{
}

//------------------------------------------------------------------------------------------------

Security::Strategy CSecurityState::GetStrategy() const
{
    std::shared_lock lock(m_mutex);
    return m_strategy;
}

//------------------------------------------------------------------------------------------------

std::string CSecurityState::GetAuthority() const 
{
    std::shared_lock lock(m_mutex);
    return m_authority;
}

//------------------------------------------------------------------------------------------------

std::string CSecurityState::GetToken() const
{
    std::shared_lock lock(m_mutex);
    return m_token;
}

//------------------------------------------------------------------------------------------------

void CSecurityState::SetStrategy(Security::Strategy strategy)
{
    std::unique_lock lock(m_mutex);
    m_strategy = strategy;
}

//------------------------------------------------------------------------------------------------

void CSecurityState::SetAuthority(std::string_view authority)
{
    std::unique_lock lock(m_mutex);
    m_authority = authority;
}

//------------------------------------------------------------------------------------------------

void CSecurityState::SetToken(std::string_view token)
{
    std::unique_lock lock(m_mutex);
    m_token = token;
}

//------------------------------------------------------------------------------------------------