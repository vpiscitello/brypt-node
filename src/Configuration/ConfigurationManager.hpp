//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "StatusCode.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <filesystem>
#include <optional>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Configuration {
//------------------------------------------------------------------------------------------------

class CManager;

//------------------------------------------------------------------------------------------------
} // Configuration namespace
//------------------------------------------------------------------------------------------------

class Configuration::CManager  {
public:
    CManager();
    explicit CManager(std::string_view filepath);

    std::optional<Configuration::TSettings> FetchSettings();

    StatusCode Serialize();
    StatusCode ValidateSettings();

    StatusCode DecodeConfigurationFile();
    StatusCode GenerateConfigurationFile();

    std::optional<Configuration::TSettings> GetCachedConfiguration() const;
    
private:
    void GetConfigurationOptionsFromUser();
    void InitializeSettings();

    std::filesystem::path m_filepath;
    Configuration::TSettings m_settings;
    bool m_validated;
};

//------------------------------------------------------------------------------------------------