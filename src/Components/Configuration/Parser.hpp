//----------------------------------------------------------------------------------------------------------------------
// File: Parser.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Options.hpp"
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

class Parser;

//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------

class Configuration::Parser final
{
public:
    Parser(std::filesystem::path const& filepath, Options::Runtime const& options);

    [[nodiscard]] StatusCode FetchOptions();
    [[nodiscard]] StatusCode Serialize();
    [[nodiscard]] StatusCode LaunchGenerator();

    [[nodiscard]] RuntimeContext GetRuntimeContext() const;
    [[nodiscard]] spdlog::level::level_enum GetVerbosityLevel() const;
    [[nodiscard]] bool UseInteractiveConsole() const;
    [[nodiscard]] bool UseBootstraps() const;
    [[nodiscard]] bool UseFilepathDeduction() const;
    [[nodiscard]] Node::SharedIdentifier const& GetNodeIdentifier() const;
    [[nodiscard]] std::string const& GetNodeName() const;
    [[nodiscard]] std::string const& GetNodeDescription() const;
    [[nodiscard]] std::string const& GetNodeLocation() const;
    [[nodiscard]] Options::Endpoints const& GetEndpointOptions() const;
    [[nodiscard]] Security::Strategy GetSecurityStrategy() const;
    [[nodiscard]] std::string const& GetCentralAuthority() const;

    [[nodiscard]] bool IsValidated() const;

private:
    [[nodiscard]] StatusCode ValidateOptions();
    [[nodiscard]] StatusCode Deserialize();

    void GetOptionsFromUser();
    bool InitializeOptions();
    
    std::shared_ptr<spdlog::logger> m_logger;

    std::string m_version;
    std::filesystem::path m_filepath;
    
    Options::Runtime m_runtime;
    Options::Identifier m_identifier;
    Options::Details m_details;
    Options::Endpoints m_endpoints;
    Options::Security m_security;

    bool m_validated;
};

//----------------------------------------------------------------------------------------------------------------------