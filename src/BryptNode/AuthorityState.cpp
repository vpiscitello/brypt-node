//------------------------------------------------------------------------------------------------
#include "AuthorityState.hpp"
//------------------------------------------------------------------------------------------------

CAuthorityState::CAuthorityState(std::string_view const& url)
    : m_mutex()
    , m_url(url)
    , m_token()
{
}

//------------------------------------------------------------------------------------------------

std::string CAuthorityState::GetUrl() const 
{
    std::shared_lock lock(m_mutex);
    return m_url;
}

//------------------------------------------------------------------------------------------------

std::string CAuthorityState::GetToken() const
{
    std::shared_lock lock(m_mutex);
    return m_token;
}

//------------------------------------------------------------------------------------------------

void CAuthorityState::SetUrl(std::string_view const& url)
{
    std::unique_lock lock(m_mutex);
    m_url = url;
}

//------------------------------------------------------------------------------------------------

void CAuthorityState::SetToken(std::string const& token)
{
    std::unique_lock lock(m_mutex);
    m_token = token;
}

//------------------------------------------------------------------------------------------------