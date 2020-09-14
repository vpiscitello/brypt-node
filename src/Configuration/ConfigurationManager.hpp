//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "StatusCode.hpp"
//------------------------------------------------------------------------------------------------
#include <filesystem>
#include <optional>
#include <string_view>
//------------------------------------------------------------------------------------------------

namespace BryptIdentifier {
    class CContainer;
}

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
    explicit CManager(TSettings const& settings);

    StatusCode FetchSettings();
    StatusCode Serialize();

    StatusCode GenerateConfigurationFile();

    std::optional<Configuration::TSettings> GetSettings() const;

    BryptIdentifier::SharedContainer GetBryptIdentifier() const;

    std::string GetNodeName() const;
    std::string GetNodeDescription() const;
    std::string GetNodeLocation() const;

    std::optional<Configuration::EndpointConfigurations> GetEndpointConfigurations() const;

    std::string GetSecurityStandard() const;
    std::string GetCentralAuthority() const;

private:
    StatusCode ValidateSettings();
    StatusCode DecodeConfigurationFile();

    void GetConfigurationOptionsFromUser();
    void InitializeSettings();

    std::filesystem::path m_filepath;
    TSettings m_settings;
    bool m_validated;
};

//------------------------------------------------------------------------------------------------