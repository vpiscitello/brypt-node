//----------------------------------------------------------------------------------------------------------------------
#include "SecurityState.hpp"
//----------------------------------------------------------------------------------------------------------------------

SecurityState::SecurityState(Security::Strategy strategy, std::string_view authority)
    : m_mutex()
    , m_strategy(strategy)
    , m_authority(authority)
    , m_token()
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::Strategy SecurityState::GetStrategy() const
{
    std::shared_lock lock(m_mutex);
    return m_strategy;
}

//----------------------------------------------------------------------------------------------------------------------

std::string SecurityState::GetAuthority() const 
{
    std::shared_lock lock(m_mutex);
    return m_authority;
}

//----------------------------------------------------------------------------------------------------------------------

std::string SecurityState::GetToken() const
{
    std::shared_lock lock(m_mutex);
    return m_token;
}

//----------------------------------------------------------------------------------------------------------------------

void SecurityState::SetStrategy(Security::Strategy strategy)
{
    std::unique_lock lock(m_mutex);
    m_strategy = strategy;
}

//----------------------------------------------------------------------------------------------------------------------

void SecurityState::SetAuthority(std::string_view authority)
{
    std::unique_lock lock(m_mutex);
    m_authority = authority;
}

//----------------------------------------------------------------------------------------------------------------------

void SecurityState::SetToken(std::string_view token)
{
    std::unique_lock lock(m_mutex);
    m_token = token;
}

//----------------------------------------------------------------------------------------------------------------------