//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <string>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CAuthorityState {
public:
    explicit CAuthorityState(std::string_view const& url);

    std::string GetUrl() const;
    std::string GetToken() const;
    
    void SetUrl(std::string_view const& url);
    void SetToken(std::string const& token);

private:
    mutable std::shared_mutex m_mutex;
    std::string m_url;  // Networking url of the central authority for the Brypt ecosystem
    std::string m_token; // Access token for the Brypt network
};

//------------------------------------------------------------------------------------------------