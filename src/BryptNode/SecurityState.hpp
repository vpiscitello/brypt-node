//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <string>
#include <shared_mutex>
//------------------------------------------------------------------------------------------------

class CSecurityState {
public:
    CSecurityState();
    explicit CSecurityState(std::string_view const& protocol);

    std::string GetProtocol() const;

    void SetProtocol(std::string const& protocol);

private:
    mutable std::shared_mutex m_mutex;
    std::string m_standard;
};

//------------------------------------------------------------------------------------------------