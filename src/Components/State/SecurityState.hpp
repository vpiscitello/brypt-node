//----------------------------------------------------------------------------------------------------------------------
// File: SecurityState.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Security/SecurityDefinitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <atomic>
#include <string>
#include <shared_mutex>
//----------------------------------------------------------------------------------------------------------------------

class SecurityState {
public:
    explicit SecurityState(Security::Strategy strategy);

    Security::Strategy GetStrategy() const;
    std::string GetToken() const;

    void SetStrategy(Security::Strategy strategy);
    void SetToken(std::string_view token);

private:
    mutable std::shared_mutex m_mutex;
    std::atomic<Security::Strategy> m_strategy;
    std::string m_token;
};

//----------------------------------------------------------------------------------------------------------------------