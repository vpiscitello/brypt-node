//----------------------------------------------------------------------------------------------------------------------
// File: Parser.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Parser.hpp"
#include "Defaults.hpp"
#include "SerializationErrors.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Core/RuntimeContext.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/Logger.hpp"
#include "Utilities/PrettyPrinter.hpp"
#include "Utilities/WideString.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/json.hpp>
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace symbols {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Version = "version";

//----------------------------------------------------------------------------------------------------------------------
} // symbols namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: JSON Schema. 
// Note: Only select portions of the Configuration::Settings object will be encoded and decoded
// into the JSON configuration file. Meaning using metajson it is possible to omit sections of 
// the struct. However, initalization is needed to fill out the other parts of the configuration
// after decoding the file.
//----------------------------------------------------------------------------------------------------------------------
// "version": String,
// "identifier": {
//     "type": String,
//     "value": Optional String
// },
// "details": {
//     "name": Optional String,
//     "description": Optional String,
//     "location": Optional String
// },
// "network": {
//   "endpoints": [{
//       "protocol": String,
//       "interface": String,
//       "binding": String,
//       "bootstrap": Optional String
//       "connection": Optional Object,
//   }],
//   "connection": {
//       "timeout": Optional String,
//       "retry": {
//         "limit": Optional Integer,
//         "interval": Optional String
//     },
//     "token": Optional String
//   },
// },
// "security": {
//     "strategy": String,
// }
//----------------------------------------------------------------------------------------------------------------------

Configuration::Parser::Parser(Options::Runtime const& options)
    : m_logger(spdlog::get(Logger::Name.data()))
    , m_version()
    , m_filepath()
    , m_runtime(options)
    , m_identifier()
    , m_details()
    , m_network()
    , m_security()
    , m_validated(false)
    , m_changed(false)
{
    assert(m_logger);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Parser::Parser(std::filesystem::path const& filepath, Options::Runtime const& options)
    : m_logger(spdlog::get(Logger::Name.data()))
    , m_version()
    , m_filepath(filepath)
    , m_runtime(options)
    , m_identifier()
    , m_details()
    , m_network()
    , m_security()
    , m_validated(false)
    , m_changed(false)
{
    assert(m_logger);
    OnFilepathChanged();
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Parser::~Parser()
{
    if (!m_filepath.empty() && m_changed) {
        [[maybe_unused]] auto const status = Serialize();
    }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Parser::FetchOptions()
{
    // If we have a filepath, we must first process the file. 
    if (auto const status = ProcessFile(); status.first != StatusCode::Success) { return status; } 
    if (auto const status = ValidateOptions(); status.first != StatusCode::Success) { return status; }

    // Update the configuration file as the initialization of options may create new values for certain options.
    auto const status = Serialize(); 
    if (status.first != StatusCode::Success) {
        #if defined(WIN32)
        m_logger->error(L"Failed to update configuration file at: {}! Reason: {}", m_filepath.c_str(), toWideString(status.second));
        #else
        m_logger->error("Failed to update configuration file at: {}! Reason: {}", m_filepath.c_str(), status.second);
        #endif
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Parser::Serialize()
{
    // If the options the options have changed, validate them to ensure they are valid values and have been initialized.
    if (m_changed) {
        if (auto const status = ValidateOptions(); status.first != StatusCode::Success) { return status; }
    }

    // If the filesystem is disabled, there is nothing to do.
    if (m_filepath.empty()) {
        m_changed = false; // Mark 
        return { StatusCode::Success, "" };
    }

    std::ofstream os(m_filepath, std::ofstream::out);
    if (os.fail()) {
        return { StatusCode::FileError, "Failed to open file." };
    }

    boost::json::object json;

    json[symbols::Version] = m_version.GetValue();

    if (auto const status = m_identifier.Write(json); status.first != StatusCode::Success) { return status; } 
    if (auto const status = m_details.Write(json); status.first != StatusCode::Success) { return status; } 
    if (auto const status = m_network.Write(json); status.first != StatusCode::Success) { return status; } 
    if (auto const status = m_security.Write(json); status.first != StatusCode::Success) { return status; } 

    JSON::PrettyPrinter{}.Format(json, os);

    os.close();
    m_changed = false; // On success, reset the changed flag to indicate all changes have been processed. 

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Parser::ValidateOptions()
{
    m_validated = false; // Explicitly disable the validation result in case anything fails. 

    if (auto const status = m_identifier.AreOptionsAllowable(); status.first != StatusCode::Success) { return status; }
    if (auto const status = m_details.AreOptionsAllowable(); status.first != StatusCode::Success) { return status; }
    if (auto const status = m_network.AreOptionsAllowable(m_runtime); status.first != StatusCode::Success) { return status; }
    if (auto const status = m_security.AreOptionsAllowable(m_runtime); status.first != StatusCode::Success) { return status; }

    m_validated = true;

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path const& Configuration::Parser::GetFilepath() const
{
    assert(Assertions::Threading::IsCoreThread()); // Only the core thread should control the filepath. 
    return m_filepath;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetFilepath(std::filesystem::path const& filepath)
{
    assert(Assertions::Threading::IsCoreThread()); // Only the core thread should control the filepath. 
    m_changed = true; // Setting the changed flag to true, will cause the options to be serialized to the new file. 
    m_filepath = filepath;
    OnFilepathChanged();
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::DisableFilesystem()
{
    assert(Assertions::Threading::IsCoreThread()); // Only the core thread should control the filepath. 
    m_filepath.clear(); // This is not considered a change as it does not have serializable side effects. 
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::FilesystemDisabled() const { return m_filepath.empty(); }

//----------------------------------------------------------------------------------------------------------------------

RuntimeContext Configuration::Parser::GetRuntimeContext() const { return m_runtime.context; }

//----------------------------------------------------------------------------------------------------------------------

spdlog::level::level_enum Configuration::Parser::GetVerbosity() const { return m_runtime.verbosity; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::UseInteractiveConsole() const { return m_runtime.useInteractiveConsole; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::UseBootstraps() const { return m_runtime.useBootstraps; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::UseFilepathDeduction() const { return m_runtime.useFilepathDeduction; }

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Persistence Configuration::Parser::GetIdentifierPersistence() const
{
    return m_identifier.GetPersistence();
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Configuration::Parser::GetNodeIdentifier() const { return m_identifier.GetValue(); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeName() const { return m_details.GetName(); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeDescription() const { return m_details.GetDescription(); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeLocation() const { return m_details.GetLocation(); }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Parser::GetConnectionTimeout() const
{
    return m_network.GetConnectionTimeout();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Parser::GetConnectionRetryLimit() const { return m_network.GetConnectionRetryLimit(); }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Parser::GetConnectionRetryInterval() const
{
    return m_network.GetConnectionRetryInterval();
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoints const& Configuration::Parser::GetEndpoints() const
{
    return m_network.GetEndpoints();
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Parser::GetEndpoint(Network::BindingAddress const& binding) const
{
    return m_network.GetEndpoint(binding);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Parser::GetEndpoint(std::string_view const& uri) const
{
    return m_network.GetEndpoint(uri);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Parser::GetEndpoint(
    Network::Protocol protocol, std::string_view const& binding) const
{
    return m_network.GetEndpoint(protocol, binding);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::SupportedAlgorithms const& Configuration::Parser::GetSupportedAlgorithms() const
{
    return m_security.GetSupportedAlgorithms();
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> const& Configuration::Parser::GetNetworkToken() const { return m_network.GetToken(); }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::Validated() const { return m_validated && !m_changed; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::Changed() const { return m_changed; }

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetRuntimeContext(RuntimeContext context)
{
    m_changed = context != m_runtime.context;
    m_runtime.context = context;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetVerbosity(spdlog::level::level_enum verbosity)
{
    m_changed = verbosity != m_runtime.verbosity;
    m_runtime.verbosity = verbosity;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetUseInteractiveConsole(bool use)
{
    m_changed = m_runtime.useInteractiveConsole != use;
    m_runtime.useInteractiveConsole = use;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetUseBootstraps(bool use)
{
    m_changed = m_runtime.useBootstraps != use;
    m_runtime.useBootstraps = use;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetUseFilepathDeduction(bool use)
{
    m_changed = m_runtime.useFilepathDeduction != use;
    m_runtime.useFilepathDeduction = use;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetNodeIdentifier(Options::Identifier::Persistence persistence)
{
    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    // Additionally, setting the type should always cause a change in the identifier. 
    return m_identifier.SetIdentifier(persistence, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetNodeName(std::string_view const& name)
{
    return m_details.SetName(name, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetNodeDescription(std::string_view const& description)
{
    return m_details.SetDescription(description, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetNodeLocation(std::string_view const& location)
{
    return m_details.SetLocation(location, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetConnectionTimeout(std::chrono::milliseconds const& timeout)
{
    if (!m_network.SetConnectionTimeout(timeout, m_changed)) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetConnectionRetryLimit(std::int32_t limit)
{
    if (!m_network.SetConnectionRetryLimit(limit, m_changed)) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetConnectionRetryInterval(std::chrono::milliseconds const& interval)
{
    if (!m_network.SetConnectionRetryInterval(interval, m_changed)) { return false; }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Parser::UpsertEndpoint(Options::Endpoint&& options)
{
    options.SetRuntimeOptions(m_runtime);
    return m_network.UpsertEndpoint(std::move(options), m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Configuration::Options::Endpoint> Configuration::Parser::ExtractEndpoint(
    Network::BindingAddress const& binding)
{
    return m_network.ExtractEndpoint(binding, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Configuration::Options::Endpoint> Configuration::Parser::ExtractEndpoint(std::string_view const& uri)
{
    return m_network.ExtractEndpoint(uri, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Configuration::Options::Endpoint> Configuration::Parser::ExtractEndpoint(
    Network::Protocol protocol, std::string_view const& binding)
{
    return m_network.ExtractEndpoint(protocol, binding, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetNetworkToken(std::string_view const& token)
{
    return m_network.SetToken(token, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::ClearSupportedAlgorithms()
{
    m_security.ClearSupportedAlgorithms();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetSupportedAlgorithms(
    ::Security::ConfidentialityLevel level,
    std::vector<std::string> keyAgreements,
    std::vector<std::string> ciphers,
    std::vector<std::string> hashFunctions)
{
    return m_security.SetSupportedAlgorithmsAtLevel(level, keyAgreements, ciphers, hashFunctions, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::OnFilepathChanged()
{
    if (m_filepath.empty()) { return; } // If the filepath is empty, there is nothing to do.  

    // If we are allowed to deduce the filepath, update the configured path using the Defaults when applicable.
    if (m_runtime.useFilepathDeduction) {
        // If the filepath does not have a filename, attach the default config.json
        if (!m_filepath.has_filename()) { m_filepath = m_filepath / DefaultConfigurationFilename; }

        // If the filepath does not have a parent path, get and attach the default brypt folder
        if (!m_filepath.has_parent_path()) { m_filepath = GetDefaultBryptFolder() / m_filepath; }
    }

    // Create the folder structure for a new configuration file, if one does not exist. If we fail to create the file
    // path, log out an error and disable filesystem usage. 
    #if defined(BRYPT_SHARED)
    bool const failed = !FileUtils::CreateFolderIfNoneExist(m_filepath);
    #else
    // If running inside the console application, only create the new folder only if we can launch the generator.
    bool const failed = m_runtime.useInteractiveConsole && !FileUtils::CreateFolderIfNoneExist(m_filepath);
    #endif
    if (failed) { 
        #if defined(WIN32)
        m_logger->error(L"Failed to create the filepath at: {}!", m_filepath.c_str());
        #else
        m_logger->error("Failed to create the filepath at: {}!", m_filepath.c_str());
        #endif
        DisableFilesystem(); // If we failed to create the filepath, we are unable to use the filesystem. 
    }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Parser::ProcessFile()
{
    if (m_filepath.empty()) { return { StatusCode::Success, "" }; } // If filesystem usage is disabled, there is nothing to do.
    if (m_validated && !m_changed) { return { StatusCode::Success, "" }; } // If there are no changes, there is nothing to do.

    bool const found = std::filesystem::exists(m_filepath); // Determine if we have an existing file to process.
    // Have we already read this file, validated the results, and are there changes pending?
    bool const isPendingSerialization = found && m_validated && m_changed; 
    if (isPendingSerialization) { return { StatusCode::Success, "" }; } // If changes are pending serialzation, we should merge in old data.

    // If the file is found and this is the first time we are reading the file, the deserializer should handle
    // the rest of the file processing. 
    if (found) {
        #if defined(WIN32)
        m_logger->debug(L"Reading configuration file at: {}.", m_filepath.c_str());
        #else
        m_logger->debug("Reading configuration file at: {}.", m_filepath.c_str());
        #endif
        return Deserialize();
    }

    // If we have been built as a shared library, we need to indicate to the user that an error has occurred and
    // they need to resolve the options they have supplied.
    std::string error = [&filepath = m_filepath] () {
        std::ostringstream oss;
        oss << "Failed to locate a configuration file at: ";
        oss << filepath;
        return oss.str();
    }();

    return { StatusCode::FileError, std::move(error) };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Parser::Deserialize()
{
    // If the filepath is empty, filesystem usage has been disabled. 
    if (m_filepath.empty()) { return { StatusCode::Success, "" }; }

    try {
        constexpr boost::json::parse_options ParserOptions{
            .allow_comments = true,
            .allow_trailing_commas = true,
        };

        std::stringstream buffer;
        {
            std::ifstream reader{ m_filepath };
            if (reader.fail()) [[unlikely]] {
                return { StatusCode::FileError, "Failed to open configuration file for reading." };
            }
            buffer << reader.rdbuf(); // Read the file into the buffer stream.
        }

        auto const serialized = buffer.view();
        if (serialized.empty()) {
            return { StatusCode::DecodeError, "The configuration file is empty." };
        }

        boost::json::error_code error;
        auto const json = boost::json::parse(buffer.view(), error, boost::json::storage_ptr{}, ParserOptions).as_object();
        if (error) {
            return { StatusCode::DecodeError, "Failed to read the configuration file as valid JSON." };
        }

        // Required field parsing.
        if (auto itr = json.find(m_version.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_version.SetValueFromConfig(itr->value().as_string())) {
                    return {
                        StatusCode::InputError,
                        CreateInvalidValueMessage(m_version.GetFieldName())
                    };
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", m_version.GetFieldName())
                };
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMissingFieldMessage(m_version.GetFieldName())
            };
        }

        if (auto itr = json.find(Options::Identifier::GetFieldName()); itr != json.end()) {
            if (itr->value().is_object()) {
                auto const& value = itr->value().as_object();
                if (auto const status = m_identifier.Merge(value); status.first != StatusCode::Success) { 
                    return status;
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("object", Options::Identifier::GetFieldName())
                };
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMissingFieldMessage(Options::Identifier::GetFieldName())
            };
        }

        if (auto itr = json.find(Options::Details::GetFieldName()); itr != json.end()) {
            if (itr->value().is_object()) {
                auto const& value = itr->value().as_object();
                if (auto const status = m_details.Merge(value); status.first != StatusCode::Success) { 
                    return status;
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("object", Options::Details::GetFieldName())
                };
            }
        }
        
        if (auto itr = json.find(Options::Network::GetFieldName()); itr != json.end()) {
            if (itr->value().is_object()) {
                auto const& value = itr->value().as_object();
                if (auto const status = m_network.Merge(value, m_runtime); status.first != StatusCode::Success) { 
                    return status;
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("object", Options::Network::GetFieldName())
                };
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMissingFieldMessage(Options::Network::GetFieldName())
            };
        }

        if (auto itr = json.find(Options::Security::GetFieldName()); itr != json.end()) {
            if (itr->value().is_object()) {
                auto const& value = itr->value().as_object();
                if (auto const status = m_security.Merge(value, m_runtime); status.first != StatusCode::Success) { 
                    return status;
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("object", Options::Security::GetFieldName())
                };
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMissingFieldMessage(Options::Security::GetFieldName())
            };
        }
    } catch (...) {
        return { StatusCode::DecodeError, "Encountered an unexpected error while deserializing the configuration file." };
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------
