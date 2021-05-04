//----------------------------------------------------------------------------------------------------------------------
// File: Manager.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "StatusCode.hpp"
#include "BryptIdentifier/IdentifierTypes.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <filesystem>
#include <optional>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

namespace spdlog { class logger; }

//----------------------------------------------------------------------------------------------------------------------
namespace Configuration {
//----------------------------------------------------------------------------------------------------------------------

class Manager;

//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------

class Configuration::Manager final
{
public:
    Manager(std::filesystem::path const& filepath, bool interactive, bool shouldBuildPath = true);
    explicit Manager(Settings const& settings);

    [[nodiscard]] StatusCode FetchSettings();
    [[nodiscard]] StatusCode Serialize();

    [[nodiscard]] StatusCode GenerateConfigurationFile();

    [[nodiscard]] bool IsValidated() const;

    [[nodiscard]] Node::SharedIdentifier const& GetNodeIdentifier() const;
    [[nodiscard]] std::string const& GetNodeName() const;
    [[nodiscard]] std::string const& GetNodeDescription() const;
    [[nodiscard]] std::string const& GetNodeLocation() const;
    [[nodiscard]] EndpointsSet const& GetEndpointOptions() const;
    [[nodiscard]] Security::Strategy GetSecurityStrategy() const;
    [[nodiscard]] std::string const& GetCentralAuthority() const;

private:
    [[nodiscard]] StatusCode ValidateSettings();
    [[nodiscard]] StatusCode DecodeConfigurationFile();

    void GetSettingsFromUser();
    bool InitializeSettings();
    
    std::shared_ptr<spdlog::logger> m_spLogger;
    bool m_isGeneratorAllowed;

    std::filesystem::path m_filepath;
    Settings m_settings;
    bool m_validated;
};

//----------------------------------------------------------------------------------------------------------------------