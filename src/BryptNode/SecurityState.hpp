//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Components/Security/SecurityDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <string>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class SecurityState {
public:
    SecurityState(Security::Strategy strategy, std::string_view authority);

    Security::Strategy GetStrategy() const;
    std::string GetAuthority() const;
    std::string GetToken() const;

    void SetStrategy(Security::Strategy strategy);
    void SetAuthority(std::string_view authority);
    void SetToken(std::string_view token);

private:
    mutable std::shared_mutex m_mutex;

    Security::Strategy m_strategy;
    std::string m_authority;
    std::string m_token;
};

//------------------------------------------------------------------------------------------------