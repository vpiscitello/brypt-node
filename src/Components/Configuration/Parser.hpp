//----------------------------------------------------------------------------------------------------------------------
// File: Parser.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Field.hpp"
#include "Options.hpp"
#include "StatusCode.hpp"
#include "Components/Identifier/IdentifierTypes.hpp"
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
namespace Symbols {
//----------------------------------------------------------------------------------------------------------------------

DEFINE_FIELD_NAME(Version);

//----------------------------------------------------------------------------------------------------------------------
} // Symbols namespace
//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------

class Configuration::Parser final
{
public:

    explicit Parser(Options::Runtime const& options);
    Parser(std::filesystem::path const& filepath, Options::Runtime const& options);
    ~Parser();

    [[nodiscard]] DeserializationResult FetchOptions();
    [[nodiscard]] SerializationResult Serialize();

    std::filesystem::path const& GetFilepath() const;
    void SetFilepath(std::filesystem::path const& filepath);
    void DisableFilesystem();
    [[nodiscard]] bool FilesystemDisabled() const;

    [[nodiscard]] RuntimeContext GetRuntimeContext() const;
    [[nodiscard]] spdlog::level::level_enum GetVerbosity() const;
    [[nodiscard]] bool UseInteractiveConsole() const;
    [[nodiscard]] bool UseBootstraps() const;
    [[nodiscard]] bool UseFilepathDeduction() const;
    [[nodiscard]] Options::Identifier::Persistence GetIdentifierPersistence() const;
    [[nodiscard]] Node::SharedIdentifier const& GetNodeIdentifier() const;
    [[nodiscard]] std::string const& GetNodeName() const;
    [[nodiscard]] std::string const& GetNodeDescription() const;
    [[nodiscard]] std::string const& GetNodeLocation() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionTimeout() const;
    [[nodiscard]] std::int32_t GetConnectionRetryLimit() const;
    [[nodiscard]] std::chrono::milliseconds const& GetConnectionRetryInterval() const;
    [[nodiscard]] Options::Endpoints const& GetEndpoints() const;
    [[nodiscard]] Options::Network::FetchedEndpoint GetEndpoint(Network::BindingAddress const& binding) const;
    [[nodiscard]] Options::Network::FetchedEndpoint GetEndpoint(std::string_view const& uri) const;
    [[nodiscard]] Options::Network::FetchedEndpoint GetEndpoint(Network::Protocol protocol, std::string_view const& binding) const;
    [[nodiscard]] Options::SupportedAlgorithms const& GetSupportedAlgorithms() const;
    [[nodiscard]] std::optional<std::string> const& GetNetworkToken() const;

    [[nodiscard]] bool Validated() const;
    [[nodiscard]] bool Changed() const;

    void SetRuntimeContext(RuntimeContext context);
    void SetVerbosity(spdlog::level::level_enum verbosity);
    void SetUseInteractiveConsole(bool use);
    void SetUseBootstraps(bool use);
    void SetUseFilepathDeduction(bool use);
    [[nodiscard]] bool SetNodeIdentifier(Options::Identifier::Persistence persistence);
    [[nodiscard]] bool SetNodeName(std::string_view const& name);
    [[nodiscard]] bool SetNodeDescription(std::string_view const& description);
    [[nodiscard]] bool SetNodeLocation(std::string_view const& location);
    [[nodiscard]] bool SetConnectionTimeout(std::chrono::milliseconds const& timeout);
    [[nodiscard]] bool SetConnectionRetryLimit(std::int32_t limit);
    [[nodiscard]] bool SetConnectionRetryInterval(std::chrono::milliseconds const& interval);
    [[nodiscard]] Options::Network::FetchedEndpoint UpsertEndpoint(Options::Endpoint&& options);
    std::optional<Options::Endpoint> ExtractEndpoint(Network::BindingAddress const& binding);
    std::optional<Options::Endpoint> ExtractEndpoint(std::string_view const& uri);
    std::optional<Options::Endpoint> ExtractEndpoint(Network::Protocol protocol, std::string_view const& binding);
    [[nodiscard]] bool SetNetworkToken(std::string_view const& token);

    void ClearSupportedAlgorithms();

    [[nodiscard]] bool SetSupportedAlgorithms(
        ::Security::ConfidentialityLevel level,
        std::vector<std::string> keyAgreements,
        std::vector<std::string> ciphers,
        std::vector<std::string> hashFunctions);

private:
    void OnFilepathChanged();
    [[nodiscard]] DeserializationResult ProcessFile();
    [[nodiscard]] DeserializationResult Deserialize();

    [[nodiscard]] ValidationResult ValidateOptions();
    
    std::shared_ptr<spdlog::logger> m_logger;

    Field<Symbols::Version, std::string> m_version;
    std::filesystem::path m_filepath;
    
    Options::Runtime m_runtime;
    Options::Identifier m_identifier;
    Options::Details m_details;
    Options::Network m_network;
    Options::Security m_security;

    bool m_validated;
    bool m_changed;
};

//----------------------------------------------------------------------------------------------------------------------