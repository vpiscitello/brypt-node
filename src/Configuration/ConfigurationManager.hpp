//------------------------------------------------------------------------------------------------
// File: Parser.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Configuration {
//------------------------------------------------------------------------------------------------

std::filesystem::path const DefaultConfigFilename = "config.json";

class CManager;

//------------------------------------------------------------------------------------------------
} // Configuration namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

class Configuration::CManager {
public:
    CManager();
    explicit CManager(std::string const& filepath);
    
    enum class StatusCode { SUCCESS, DECODE_ERROR, INPUT_ERROR, FILE_ERROR };

    std::optional<Configuration::TSettings> Parse();

    StatusCode Save();
    StatusCode ValidateSettings();

    StatusCode DecodeConfigurationFile();
    StatusCode GenerateConfigurationFile();

    std::filesystem::path GetDefaultConfigurationPath() const;

    std::optional<Configuration::TSettings> GetConfiguration() const;
    
private:
    void CreateConfigurationFilePath();
    void GetConfigurationOptionsFromUser();
    void InitializeSettings();

    std::filesystem::path m_filepath;
    Configuration::TSettings m_settings;
    bool m_validated;
};

//------------------------------------------------------------------------------------------------