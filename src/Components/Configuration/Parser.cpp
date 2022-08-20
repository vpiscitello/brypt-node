//----------------------------------------------------------------------------------------------------------------------
// File: Parser.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Parser.hpp"
#include "Defaults.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/RuntimeContext.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/Logger.hpp"
#include "Utilities/PrettyPrinter.hpp"
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
    : m_logger(spdlog::get(Logger::Name::Core.data()))
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
    : m_logger(spdlog::get(Logger::Name::Core.data()))
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

Configuration::StatusCode Configuration::Parser::FetchOptions()
{
    // If we have a filepath, we must first process the file. 
    if (auto const status = ProcessFile(); status != StatusCode::Success) { return status; } 
    if (auto const status = ValidateOptions(); status != StatusCode::Success) { return status; }
    
    // Update the configuration file as the initialization of options may create new values for certain options.
    if (auto const status = Serialize(); status != StatusCode::Success) {
        #if defined(WIN32)
        m_logger->error(L"Failed to update configuration file at: {}!", fmt::to_string_view(m_filepath.c_str()));
        #else
        m_logger->error("Failed to update configuration file at: {}!", m_filepath.c_str());
        #endif
        return status;
    }

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::Serialize()
{
    // If the options the options have changed, validate them to ensure they are valid values and have been initialized.
    if (m_changed) {
        if (auto const status = ValidateOptions(); status != StatusCode::Success) { return status; }
    }

    // If the filesystem is disabled, there is nothing to do.
    if (m_filepath.empty()) {
        m_changed = false; // Mark 
        return StatusCode::Success;
    }

    std::ofstream os(m_filepath, std::ofstream::out);
    if (os.fail()) { return StatusCode::FileError; }

    boost::json::object json;

    json[symbols::Version] = m_version;
    m_identifier.Write(json);
    m_details.Write(json);
    m_network.Write(json);
    m_security.Write(json);

    JSON::PrettyPrinter{}.Format(json, os);

    os.close();
    m_changed = false; // On success, reset the changed flag to indicate all changes have been processed. 

    return StatusCode::Success;
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

Configuration::Options::Identifier::Type Configuration::Parser::GetIdentifierType() const { return m_identifier.GetType(); }

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

Security::Strategy Configuration::Parser::GetSecurityStrategy() const { return m_security.GetStrategy(); }

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

bool Configuration::Parser::SetNodeIdentifier(Options::Identifier::Type type)
{
    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    // Additionally, setting the type should always cause a change in the identifier. 
    return m_identifier.SetIdentifier(type, m_changed, m_logger);
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
    return m_network.UpsertEndpoint(std::move(options), m_runtime, m_logger, m_changed);
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

void Configuration::Parser::SetSecurityStrategy(Security::Strategy strategy)
{
    m_security.SetStrategy(strategy, m_changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::SetNetworkToken(std::string_view const& token)
{
    return m_network.SetToken(token, m_changed);
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
        m_logger->error(L"Failed to create the filepath at: {}!", fmt::to_string_view(m_filepath.c_str()));
        #else
        m_logger->error("Failed to create the filepath at: {}!", m_filepath.c_str());
        #endif
        DisableFilesystem(); // If we failed to create the filepath, we are unable to use the filesystem. 
    }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::ProcessFile()
{
    if (m_filepath.empty()) { return StatusCode::Success; } // If filesystem usage is disabled, there is nothing to do.
    if (m_validated && !m_changed) { return StatusCode::Success; } // If there are no changes, there is nothing to do.

    bool const found = std::filesystem::exists(m_filepath); // Determine if we have an existing file to process.
    bool const updated = m_changed && m_validated && found; // Have read a file and are now applying changes?
    if (updated) { return StatusCode::Success; } // If either condition is true, there is nothing to do.

    // If the file is found and this is the first time we are reading the file, the deserializer should handle
    // the rest of the file processing. 
    if (found) {
        #if defined(WIN32)
        m_logger->debug(L"Reading configuration file at: {}.", fmt::to_string_view(m_filepath.c_str()));
        #else
        m_logger->debug("Reading configuration file at: {}.", m_filepath.c_str());
        #endif
        return Deserialize();
    }

    // If we have been built as a shared library, we need to indicate to the user that an error has occured and
    // they need to resolve the options they have supplied. 
    #if defined(WIN32)
    m_logger->error(L"Failed to locate a configuration file at: {}", fmt::to_string_view(m_filepath.c_str()));
    #else
    m_logger->error("Failed to locate a configuration file at: {}", m_filepath.c_str());
    #endif

    return StatusCode::FileError;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::ValidateOptions()
{
    m_validated = false; // Explicitly disable the validation result in case anything fails. 

    if (!AreOptionsAllowable()) { return StatusCode::DecodeError; }

    if (!m_identifier.Initialize(m_logger)) { return StatusCode::InputError; }
    if (!m_details.Initialize(m_logger)) { return StatusCode::InputError; }
    if (!m_network.Initialize(m_runtime, m_logger)) { return StatusCode::InputError; }
    if (!m_security.Initialize(m_logger)) { return StatusCode::InputError; }

    m_validated = true;

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::AreOptionsAllowable() const
{
    if (!m_identifier.IsAllowable()) { return false; }
    if (!m_network.IsAllowable(m_runtime)) { return false; }
    if (!m_security.IsAllowable()) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::Deserialize()
{
    // If the filepath is empty, filesystem usage has been disabled. 
    if (m_filepath.empty()) { return StatusCode::Success; }

    try {
        constexpr boost::json::parse_options ParserOptions{
            .allow_comments = true,
            .allow_trailing_commas = true,
        };

        std::stringstream buffer;
        {
            std::ifstream reader{ m_filepath };
            if (reader.fail()) [[unlikely]] {
                return StatusCode::FileError;
            }
            buffer << reader.rdbuf(); // Read the file into the buffer stream.
        }

        auto const serialized = buffer.view();
        if (serialized.empty()) {
            return StatusCode::InputError;
        }

        boost::json::error_code error;
        auto const json = boost::json::parse(buffer.view(), error, boost::json::storage_ptr{}, ParserOptions).as_object();
        if (error) {
            return StatusCode::DecodeError;
        }

        // Required field parsing.
        m_version = json.at(symbols::Version).as_string();
        m_identifier.Merge(json.at(Options::Identifier::Symbol).as_object());
        m_network.Merge(json.at(Options::Network::Symbol).as_object());
        m_security.Merge(json.at(Options::Security::Symbol).as_object());

        // Optional field parsing.
        if (auto const itr = json.find(Options::Details::Symbol); itr != json.end()) {
            m_details.Merge(itr->value().as_object());
        }

    } catch (...) {
        return StatusCode::DecodeError;
    }

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------
