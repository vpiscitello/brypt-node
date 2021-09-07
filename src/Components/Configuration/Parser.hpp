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
    using FetchedEndpoint = std::optional<std::reference_wrapper<Options::Endpoint const>>;

    explicit Parser(Options::Runtime const& options);
    Parser(std::filesystem::path const& filepath, Options::Runtime const& options);
    ~Parser();

    [[nodiscard]] StatusCode FetchOptions();
    [[nodiscard]] StatusCode Serialize();
    [[nodiscard]] StatusCode LaunchGenerator();

    std::filesystem::path const& GetFilepath() const;
    void SetFilepath(std::filesystem::path const& filepath);
    void DisableFilesystem();

    [[nodiscard]] RuntimeContext GetRuntimeContext() const;
    [[nodiscard]] spdlog::level::level_enum GetVerbosity() const;
    [[nodiscard]] bool UseInteractiveConsole() const;
    [[nodiscard]] bool UseBootstraps() const;
    [[nodiscard]] bool UseFilepathDeduction() const;
    [[nodiscard]] Options::Identifier::Type GetIdentifierType() const;
    [[nodiscard]] Node::SharedIdentifier const& GetNodeIdentifier() const;
    [[nodiscard]] std::string const& GetNodeName() const;
    [[nodiscard]] std::string const& GetNodeDescription() const;
    [[nodiscard]] std::string const& GetNodeLocation() const;
    [[nodiscard]] Options::Endpoints const& GetEndpoints() const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(Network::BindingAddress const& binding) const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(std::string_view const& uri) const;
    [[nodiscard]] FetchedEndpoint GetEndpoint(Network::Protocol protocol, std::string_view const& binding) const;
    [[nodiscard]] Security::Strategy GetSecurityStrategy() const;
    [[nodiscard]] std::string const& GetNetworkToken() const;

    [[nodiscard]] bool Validated() const;
    [[nodiscard]] bool Changed() const;
    [[nodiscard]] bool FilesystemDisabled() const;

    void SetRuntimeContext(RuntimeContext context);
    void SetVerbosity(spdlog::level::level_enum level);
    void SetUseInteractiveConsole(bool use);
    void SetUseBootstraps(bool use);
    void SetUseFilepathDeduction(bool use);
    void SetNodeIdentifier(Options::Identifier::Type type);
    void SetNodeName(std::string_view const& name);
    void SetNodeDescription(std::string_view const& description);
    void SetNodeLocation(std::string_view const& location);
    [[nodiscard]] bool UpsertEndpoint(Options::Endpoint&& options);
    [[nodiscard]] bool RemoveEndpoint(Network::BindingAddress const& binding);
    [[nodiscard]] bool RemoveEndpoint(std::string_view const& uri);
    [[nodiscard]] bool RemoveEndpoint(Network::Protocol protocol, std::string_view const& binding);
    void SetSecurityStrategy(Security::Strategy strategy);
    void SetNetworkToken(std::string_view const& token);

private:
    void OnFilepathChanged();
    [[nodiscard]] StatusCode ProcessFile();
    [[nodiscard]] StatusCode ValidateOptions();
    [[nodiscard]] bool AreOptionsAllowable() const;
    
    [[nodiscard]] StatusCode Deserialize();

    void GetOptionsFromUser();
    
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