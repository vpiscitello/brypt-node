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
    Parser();
    Parser(std::filesystem::path const& filepath, Options::Runtime const& options);
    ~Parser();

    [[nodiscard]] StatusCode FetchOptions();
    [[nodiscard]] StatusCode Serialize();
    [[nodiscard]] StatusCode LaunchGenerator();

    void SetFilepath(std::filesystem::path const& filepath);
    void DisableFilesystem();

    [[nodiscard]] RuntimeContext GetRuntimeContext() const;
    [[nodiscard]] spdlog::level::level_enum GetVerbosity() const;
    [[nodiscard]] bool UseInteractiveConsole() const;
    [[nodiscard]] bool UseBootstraps() const;
    [[nodiscard]] bool UseFilepathDeduction() const;
    [[nodiscard]] Node::SharedIdentifier const& GetNodeIdentifier() const;
    [[nodiscard]] std::string const& GetNodeName() const;
    [[nodiscard]] std::string const& GetNodeDescription() const;
    [[nodiscard]] std::string const& GetNodeLocation() const;
    [[nodiscard]] Options::Endpoints const& GetEndpointOptions() const;
    [[nodiscard]] Security::Strategy GetSecurityStrategy() const;
    [[nodiscard]] std::string const& GetNetworkToken() const;

    [[nodiscard]] bool Validated() const;
    [[nodiscard]] bool Changed() const;

    void SetRuntimeContext(RuntimeContext context);
    void SetVerbosity(spdlog::level::level_enum level);
    void SetUseBootstraps(bool used);
    void SetIdentifierType(Options::Identifier::Type type);
    void SetNodeName(std::string_view const& name);
    void SetNodeDescription(std::string_view const& description);
    void SetNodeLocation(std::string_view const& location);
    [[nodiscard]] bool UpsertEndpoint(Options::Endpoint&& options);
    [[nodiscard]] bool RemoveEndpoint(Network::BindingAddress const& binding);
    void SetSecurityStrategy(Security::Strategy strategy);
    void SetNetworkToken(std::string_view const& token);

private:
    void OnFilepathChanged();
    [[nodiscard]] StatusCode ProcessFile();
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
    bool m_changed;
};

//----------------------------------------------------------------------------------------------------------------------