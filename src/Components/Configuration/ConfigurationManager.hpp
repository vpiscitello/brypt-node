//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "StatusCode.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <filesystem>
#include <optional>
#include <string_view>
//------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//------------------------------------------------------------------------------------------------
namespace Configuration {
//------------------------------------------------------------------------------------------------

class Manager;

//------------------------------------------------------------------------------------------------
} // Configuration namespace
//------------------------------------------------------------------------------------------------

class Configuration::Manager  {
public:
    Manager();
    explicit Manager(std::string_view filepath);
    explicit Manager(Settings const& settings);

    StatusCode FetchSettings();
    StatusCode Serialize();

    StatusCode GenerateConfigurationFile();

    std::optional<Configuration::Settings> GetSettings() const;

    BryptIdentifier::SharedContainer GetBryptIdentifier() const;

    std::string GetNodeName() const;
    std::string GetNodeDescription() const;
    std::string GetNodeLocation() const;

    std::optional<Configuration::EndpointConfigurations> GetEndpointConfigurations() const;

    Security::Strategy GetSecurityStrategy() const;
    std::string GetCentralAuthority() const;

private:
    StatusCode ValidateSettings();
    StatusCode DecodeConfigurationFile();

    void GetConfigurationOptionsFromUser();
    void InitializeSettings();
    
    std::shared_ptr<spdlog::logger> m_spLogger;

    std::filesystem::path m_filepath;
    Settings m_settings;
    bool m_validated;
};

//------------------------------------------------------------------------------------------------