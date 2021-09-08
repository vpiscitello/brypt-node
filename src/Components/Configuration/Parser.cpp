//----------------------------------------------------------------------------------------------------------------------
// File: Parser.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Parser.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/RuntimeContext.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/Assertions.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/Logger.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/algorithm/string.hpp>
#include <lithium_json.hh>
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

struct OptionsStore;

void SerializeVersion(std::ofstream& out);
void SerializeIdentifier(Configuration::Options::Identifier const& options, std::ofstream& out);
void SerializeDetail(Configuration::Options::Details const& options, std::ofstream& out);
void SerializeEndpoints(Configuration::Options::Endpoints const& endpoints, std::ofstream& out);
void SerializeSecurity(Configuration::Options::Security const& options, std::ofstream& out);

Configuration::Options::Identifier GetIdentifierFromUser();
Configuration::Options::Details GetDetailFromUser();
Configuration::Options::Endpoints GetEndpointFromUser();
Configuration::Options::Security GetSecurityFromUser();

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace defaults {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::uint32_t FileSizeLimit = 12'000; // Limit the configuration files to 12KB

constexpr std::string_view IdentifierType = "Persistent";

constexpr std::string_view EndpointType = "TCP";
constexpr std::string_view NetworkInterface = "lo";
constexpr std::string_view TcpBindingAddress = "*:35216";
constexpr std::string_view TcpBootstrapEntry = "127.0.0.1:35216";
constexpr std::string_view LoRaBindingAddress = "915:71";

constexpr std::string_view SecurityStrategy = "PQNISTL3";
constexpr std::string_view NetworkToken = "";

//----------------------------------------------------------------------------------------------------------------------
} // default namespace
//----------------------------------------------------------------------------------------------------------------------
namespace allowable {
//----------------------------------------------------------------------------------------------------------------------

std::array<std::string_view, 2> IdentifierTypes = { "Ephemeral", "Persistent" };
std::array<std::string_view, 2> EndpointTypes = { "TCP" };
std::array<std::string_view, 1> StrategyTypes = { "PQNISTL3" };

template <typename ArrayType, std::size_t ArraySize>
std::optional<std::string> IfAllowableGetValue(
    std::array<ArrayType, ArraySize> const& values,
    std::string_view value)
{
    if (value.empty()) { return {}; }

    auto const found = std::ranges::find_if(values, [&value] (std::string_view const& entry) { 
        return (boost::iequals(value, entry)); 
    });

    if (found == values.end()) { return {}; }
    return std::string(found->begin(), found->end());
}

template <typename ArrayType, std::size_t ArraySize>
void OutputValues(std::array<ArrayType, ArraySize> const& values)
{
    std::cout << "[";
    for (std::uint32_t idx = 0; idx < values.size(); ++idx) {
        std::cout << "\"" << values[idx] << "\"";
        if (idx != values.size() - 1) { std::cout << ", "; }
    }
    std::cout << "]" << std::endl;
}

//----------------------------------------------------------------------------------------------------------------------
} // default namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding. 
// Note: Only select portions of the Configuration::Settings object will be encoded and decoded
// into the JSON configuration file. Meaning using metajson it is possible to omit sections of 
// the struct. However, initalization is needed to fill out the other parts of the configuration
// after decoding the file.
//----------------------------------------------------------------------------------------------------------------------
#ifndef LI_SYMBOL_version
#define LI_SYMBOL_version
LI_SYMBOL(version)
#endif
// "version": String

#ifndef LI_SYMBOL_identifier
#define LI_SYMBOL_identifier
LI_SYMBOL(identifier)
#endif
// "identifier": {
//     "value": Optional String,
//     "type": String,
// }

#ifndef LI_SYMBOL_details
#define LI_SYMBOL_details
LI_SYMBOL(details)
#endif
// "details": {
//     "name": String,
//     "description": String,
//     "location": String
// }

#ifndef LI_SYMBOL_endpoints
#define LI_SYMBOL_endpoints
LI_SYMBOL(endpoints)
#endif
// "endpoints": [{
//     "type": String,
//     "interface": String,
//     "binding": String,
//     "bootstrap": Optional String
// }]

#ifndef LI_SYMBOL_security
#define LI_SYMBOL_security
LI_SYMBOL(security)
#endif
// "security": {
//     "strategy": String,
//     "token": String
// }

#ifndef LI_SYMBOL_binding
#define LI_SYMBOL_binding
LI_SYMBOL(binding)
#endif
#ifndef LI_SYMBOL_bootstrap
#define LI_SYMBOL_bootstrap
LI_SYMBOL(bootstrap)
#endif
#ifndef LI_SYMBOL_description
#define LI_SYMBOL_description
LI_SYMBOL(description)
#endif
#ifndef LI_SYMBOL_interface
#define LI_SYMBOL_interface
LI_SYMBOL(interface)
#endif
#ifndef LI_SYMBOL_location
#define LI_SYMBOL_location
LI_SYMBOL(location)
#endif
#ifndef LI_SYMBOL_name
#define LI_SYMBOL_name
LI_SYMBOL(name)
#endif
#ifndef LI_SYMBOL_strategy
#define LI_SYMBOL_strategy
LI_SYMBOL(strategy)
#endif
#ifndef LI_SYMBOL_protocol
#define LI_SYMBOL_protocol
LI_SYMBOL(protocol)
#endif
#ifndef LI_SYMBOL_tokenLI_SYMBOL_token
#define LI_SYMBOL_tokenLI_SYMBOL_token
LI_SYMBOL(token)
#endif
#ifndef LI_SYMBOL_type
#define LI_SYMBOL_type
LI_SYMBOL(type)
#endif
#ifndef LI_SYMBOL_value
#define LI_SYMBOL_value
LI_SYMBOL(value)
#endif

//----------------------------------------------------------------------------------------------------------------------

struct local::OptionsStore
{
    std::string version;
    Configuration::Options::Identifier identifier;
    Configuration::Options::Details details;
    Configuration::Options::Endpoints endpoints;
    Configuration::Options::Security security;
};

//----------------------------------------------------------------------------------------------------------------------

Configuration::Parser::Parser(Options::Runtime const& options)
    : m_logger(spdlog::get(Logger::Name::Core.data()))
    , m_version()
    , m_filepath()
    , m_runtime(options)
    , m_identifier()
    , m_details()
    , m_endpoints()
    , m_security()
    , m_validated(false)
    , m_changed(false)
{
    assert(m_logger);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Parser::Parser(std::filesystem::path const& filepath, Options::Runtime const& runtimeOptions)
    : m_logger(spdlog::get(Logger::Name::Core.data()))
    , m_version()
    , m_filepath(filepath)
    , m_runtime(runtimeOptions)
    , m_identifier()
    , m_details()
    , m_endpoints()
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
        assert(status == Configuration::StatusCode::Success); 
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
        m_logger->error("Failed to update configuration file at: {}!", m_filepath.c_str());
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

    std::ofstream out(m_filepath, std::ofstream::out);
    if (out.fail()) { return StatusCode::FileError; }

    out << "{\n";

    local::SerializeVersion(out);
    local::SerializeIdentifier(m_identifier, out);
    local::SerializeDetail(m_details, out);
    local::SerializeEndpoints(m_endpoints, out);
    local::SerializeSecurity(m_security, out);

    out << "}" << std::flush;

    out.close();
    m_changed = false; // On success, reset the changed flag to indicate all changes have been processed. 

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------
// Description: Generates a new configuration file based on user handler line arguements or from user input.
//----------------------------------------------------------------------------------------------------------------------
Configuration::StatusCode Configuration::Parser::LaunchGenerator()
{
    GetOptionsFromUser();

    // If we could not validate the user provided options, early return the error code.
    if (StatusCode status = ValidateOptions(); status != StatusCode::Success) { return status; }

    // If for some reason we could not serialize the options 
    if (StatusCode status = Serialize(); status != StatusCode::Success) {
        m_logger->error("Failed to save configuration options to: {}!", m_filepath.c_str());
        return status;
    }

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

Configuration::Options::Identifier::Type Configuration::Parser::GetIdentifierType() const { return m_identifier.constructed.type; }

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Configuration::Parser::GetNodeIdentifier() const { return m_identifier.constructed.value; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeName() const { return m_details.name; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeDescription() const { return m_details.description; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeLocation() const { return m_details.location; }

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoints const& Configuration::Parser::GetEndpoints() const { return m_endpoints; }

//----------------------------------------------------------------------------------------------------------------------

Configuration::Parser::FetchedEndpoint Configuration::Parser::GetEndpoint(Network::BindingAddress const& binding) const
{
    auto const itr = std::ranges::find_if(m_endpoints, [&binding] (Options::Endpoint const& existing) {
        return binding == existing.constructed.binding;
    });

    if (itr == m_endpoints.end()) { return {}; }
    return *itr;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Parser::FetchedEndpoint Configuration::Parser::GetEndpoint(std::string_view const& uri) const
{
    auto const itr = std::ranges::find_if(m_endpoints, [&uri] (Options::Endpoint const& existing) {
        return uri == existing.constructed.binding.GetUri();
    });

    if (itr == m_endpoints.end()) { return {}; }
    return *itr;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Parser::FetchedEndpoint Configuration::Parser::GetEndpoint(
    Network::Protocol protocol, std::string_view const& binding) const
{
    auto const itr = std::ranges::find_if(m_endpoints, [&protocol, &binding] (Options::Endpoint const& existing) {
        return protocol == existing.constructed.protocol && binding == existing.binding;
    });

    if (itr == m_endpoints.end()) { return {}; }
    return *itr;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Strategy Configuration::Parser::GetSecurityStrategy() const { return m_security.constructed.strategy; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNetworkToken() const { return m_security.token; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::Validated() const { return m_validated && !m_changed; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::Changed() const { return m_changed; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::FilesystemDisabled() const { return m_filepath.empty(); }

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetRuntimeContext(RuntimeContext context)
{
    m_changed = true;
    m_runtime.context = context;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetVerbosity(spdlog::level::level_enum level)
{
    m_changed = true;
    m_runtime.verbosity = level;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetUseInteractiveConsole(bool use)
{
    m_changed = true;
    m_runtime.useInteractiveConsole = use;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetUseBootstraps(bool use)
{
    m_changed = true;
    m_runtime.useBootstraps = use;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetUseFilepathDeduction(bool use)
{
    m_changed = true;
    m_runtime.useFilepathDeduction = use;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetNodeIdentifier(Options::Identifier::Type type)
{
    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    m_changed = true;
    m_identifier.constructed.type = type;
    switch (type) {
        case Options::Identifier::Type::Ephemeral: m_identifier.type = "Ephemeral"; break;
        case Options::Identifier::Type::Persistent: m_identifier.type = "Persistent"; break;
        default: assert(false); break;
    }

     // Currently, changes  to the identifier type will always update the actual identifier. 
    [[maybe_unused]] bool const initialized = m_identifier.Initialize();
    assert(initialized); // We control the "parsed" values, so initialization should never fail. 
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetNodeName(std::string_view const& name)
{
    m_changed = true;
    m_details.name = name;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetNodeDescription(std::string_view const& description)
{
    m_changed = true;
    m_details.description = description;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetNodeLocation(std::string_view const& location)
{
    m_changed = true;
    m_details.location = location;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::UpsertEndpoint(Options::Endpoint&& options)
{
    if (!options.Initialize()) { return false; } // If the provided options are invalid, return false. 

    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    m_changed = true;
    auto const itr = std::ranges::find_if(m_endpoints, [&options] (Options::Endpoint const& existing) {
        return options.binding == existing.binding;
    });

    // If options for the binding were found, update the stored value. Otherwise, add the options to endpoints set.
    if (itr != m_endpoints.end()) { *itr = std::move(options); } else { m_endpoints.emplace_back(options); }
    return true; 
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::RemoveEndpoint(Network::BindingAddress const& binding)
{
    m_changed = true;
    auto const count = std::erase_if(m_endpoints, [&binding] (Options::Endpoint const& existing) {
        return binding == existing.constructed.binding;
    });
    return count == 1;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::RemoveEndpoint(std::string_view const& uri)
{
    m_changed = true;
    auto const count = std::erase_if(m_endpoints, [&uri] (Options::Endpoint const& existing) {
        return uri == existing.constructed.binding.GetUri();
    });
    return count == 1;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::RemoveEndpoint(Network::Protocol protocol, std::string_view const& binding)
{
    m_changed = true;
    auto const count = std::erase_if(m_endpoints, [&protocol, &binding] (Options::Endpoint const& existing) {
        return protocol == existing.constructed.protocol && binding == existing.binding;
    });
    return count == 1;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetSecurityStrategy(Security::Strategy strategy)
{
    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    m_changed = true;
    m_security.constructed.strategy = strategy;
    switch (strategy) {
        case Security::Strategy::PQNISTL3: m_security.strategy = "PQNISTL3"; break;
        default: assert(false); break;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::SetNetworkToken(std::string_view const& token)
{
    m_changed = true;
    m_security.token = token;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::OnFilepathChanged()
{
    if (m_filepath.empty()) { return; } // If the filepath is empty, there is nothing to do.  

    // If we are allowed to deduce the filepath, update the configured path using the defaults when applicable.
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
        m_logger->error("Failed to creatSe the filepath at: {}!", m_filepath.c_str());
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
        m_logger->debug("Reading configuration file at: {}.", m_filepath.c_str());
        return Deserialize();
    }

    // If we have reached this point, we couldn't quite determine what we should do. The resolution is dependent
    // the context of the binary. 
    #if defined(BRYPT_SHARED)
    // If we have been built as a shared library, we need to indicate to the user that an error has occured and
    // they need to resolve the options they have supplied. 
    m_logger->error("Failed to locate a configuration file at: {}", m_filepath.c_str());
    #else
    // If we have been built as a console application, we may be able to launch the command line configuration 
    // generator to initialize the options. 
    m_logger->warn("Failed to locate a configuration file at: {}", m_filepath.c_str());
    bool const create = m_runtime.useInteractiveConsole && !m_validated && !found; // Can we launch the generator?
    if (create) {
        m_logger->info("Launching configuration generator for: {}.", m_filepath.c_str());
        return LaunchGenerator();
    }
    m_logger->error("Unable to launch the configuration generator with \"--non-interactive\" enabled.");
    #endif

    return StatusCode::FileError;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::ValidateOptions()
{
    m_validated = false; // Explicitly disable the validation result in case anything fails. 

    if (!AreOptionsAllowable()) { return StatusCode::DecodeError; }

    if (!m_identifier.Initialize()) { return StatusCode::InputError; }
    if (!m_security.Initialize()) { return StatusCode::InputError; }

    m_validated = std::ranges::all_of(m_endpoints, [&logger = m_logger] (auto& options) {
        bool const success = options.Initialize();
        if (!success) {
            logger->warn("Unable to initialize the endpoint configuration for {}", options.GetProtocolString());
        }
        return success;
    });

    return (m_validated) ? StatusCode::Success : StatusCode::InputError;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::AreOptionsAllowable() const
{
    if (m_identifier.type.empty()) { return false; }
    if (!allowable::IfAllowableGetValue(allowable::IdentifierTypes, m_identifier.type)) { return false; }

    if (m_endpoints.empty()) { return false; }
    for (auto const& endpoint: m_endpoints) {
        if (!allowable::IfAllowableGetValue(allowable::EndpointTypes, endpoint.protocol)) { return false; }
    }

    if (!allowable::IfAllowableGetValue(allowable::StrategyTypes, m_security.strategy)) { return false; }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::Deserialize()
{
    // If the filepath is empty, filesystem usage has been disabled. 
    if (m_filepath.empty()) { return StatusCode::Success; }

    // Determine the size of the file about to be read. Do not read a file that is above the given treshold. 
    {
        std::error_code error;
        auto const size = std::filesystem::file_size(m_filepath, error);
        if (error || size == 0 || size > defaults::FileSizeLimit) [[unlikely]] {
            return StatusCode::FileError;
        }
    }

    std::stringstream buffer;
    std::ifstream reader(m_filepath);
    if (reader.fail()) [[unlikely]] { return StatusCode::FileError; }
    buffer << reader.rdbuf(); // Read the file into the buffer stream
    std::string_view json = buffer.view();

    // Decode the JSON string into the configuration struct
    local::OptionsStore store;
    auto const error = li::json_object(
        s::version,
        s::identifier = li::json_object(s::type, s::value),
        s::details = li::json_object(s::name, s::description, s::location),
        s::endpoints = li::json_vector(s::protocol, s::interface, s::binding, s::bootstrap),
        s::security = li::json_object(s::strategy, s::token))
        .decode(json, store);
    if (error.code) { return StatusCode::DecodeError; }

    m_version = std::move(store.version);
    m_identifier.Merge(store.identifier);
    m_details.Merge(store.details);
    m_security.Merge(store.security);
    
    std::ranges::for_each(store.endpoints, [this] (Options::Endpoint& options) {
        auto const itr = std::ranges::find_if(m_endpoints, [&options] (Options::Endpoint const& existing) {
            return options.binding == existing.binding;
        });
        if (itr == m_endpoints.end()) { m_endpoints.emplace_back(options); } else { itr->Merge(options); }
    });

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::GetOptionsFromUser()
{
    std::cout << "Generating Brypt Node Configuration Settings." << std::endl; 
    std::cout << "Please Enter your Desired Network Options.\n" << std::endl;
    std::cin.clear(); std::cin.sync();

    m_identifier = local::GetIdentifierFromUser();
    m_details = local::GetDetailFromUser();
    m_endpoints = local::GetEndpointFromUser();
    m_security = local::GetSecurityFromUser();
}

//----------------------------------------------------------------------------------------------------------------------

void local::SerializeVersion(std::ofstream& out)
{
    out << "\t\"version\": \"" << Brypt::Version << "\",\n";
}

//----------------------------------------------------------------------------------------------------------------------

void local::SerializeIdentifier(Configuration::Options::Identifier const& options, std::ofstream& out)
{
    out << "\t\"identifier\": {\n";
    out << "\t\t\"type\": \"" << options.type;
    if (options.value && options.type == "Persistent") {
        out << "\",\n\t\t\"value\": \"" << *options.value << "\"\n";
    } else {
        out << "\"\n";
    }
    out << "\t},\n";
}

//----------------------------------------------------------------------------------------------------------------------

void local::SerializeDetail(Configuration::Options::Details const& options, std::ofstream& out)
{
    out << "\t\"details\": {\n";
    out << "\t\t\"name\": \"" << options.name << "\",\n";
    out << "\t\t\"description\": \"" << options.description << "\",\n";
    out << "\t\t\"location\": \"" << options.location << "\"\n";
    out << "\t},\n";
}

//----------------------------------------------------------------------------------------------------------------------

void local::SerializeEndpoints(Configuration::Options::Endpoints const& endpoints, std::ofstream& out)
{
    out << "\t\"endpoints\": [\n";
    for (auto const& options: endpoints) {
        out << "\t\t{\n";
        out << "\t\t\t\"protocol\": \"" << options.protocol << "\",\n";
        out << "\t\t\t\"interface\": \"" << options.interface << "\",\n";
        out << "\t\t\t\"binding\": \"" << options.binding;
        if (options.bootstrap) {
            out << "\",\n\t\t\t\"bootstrap\": \"" << *options.bootstrap << "\"\n";
        } else {
            out << "\"\n";
        }
        out << "\t\t}";
        if (&options != &endpoints.back()) { out << ",\n"; }
    }
    out << "\n\t],\n";
}

//----------------------------------------------------------------------------------------------------------------------

void local::SerializeSecurity(Configuration::Options::Security const& options, std::ofstream& out)
{
    out << "\t\"security\": {\n";
    out << "\t\t\"strategy\": \"" << options.strategy << "\",\n";
    out << "\t\t\"token\": \"" << options.token << "\"\n";
    out << "\t}\n";
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier local::GetIdentifierFromUser()
{
    Configuration::Options::Identifier options{ defaults::IdentifierType };
    bool allowable = true; // Ensure the loop condition is initialized for the exit condition.
    do {
        // Get the network interface that the node will be bound too
        allowable = true;
        std::string type{ defaults::IdentifierType };
        std::cout << "Identifier Type: (" << defaults::IdentifierType << ") " << std::flush;
        std::getline(std::cin, type);
        if (!type.empty()) {
            if (auto const optValue = allowable::IfAllowableGetValue(allowable::IdentifierTypes, type); optValue) {
                options.type = *optValue;
            } else {
                std::cout << "Specified identifier type is not allowed! Allowable types include: ";
                allowable::OutputValues(allowable::IdentifierTypes);
                allowable = false;
            }
        }
        std::cout << std::endl;
    } while(!allowable);

    return options;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Details local::GetDetailFromUser()
{
    Configuration::Options::Details options;
    
    std::string name = "";
    std::cout << "Node Name: " << std::flush;
    std::getline(std::cin, name);
    if (!name.empty()) { options.name = name; }

    std::string decription = "";
    std::cout << "Node Description: " << std::flush;
    std::getline(std::cin, decription);
    if (!decription.empty()) { options.description = decription; }

    std::string location = "";
    std::cout << "Node Location: " << std::flush;
    std::getline(std::cin, location);
    if (!location.empty()) { options.location = location; }

    std::cout << "\n";

    return options;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoints local::GetEndpointFromUser()
{
    Configuration::Options::Endpoints endpoints;

    bool addEndpoint = false;
    do {
        Configuration::Options::Endpoint options{
            defaults::EndpointType, defaults::NetworkInterface, defaults::TcpBindingAddress };

        bool allowable = true;
        std::string protocol = ""; 
        std::cout << "EndpointType: (" << defaults::EndpointType << ") " << std::flush;
        std::getline(std::cin, protocol); // Get the desired primary protocol type for the node
        if (!protocol.empty()) {
            if (auto const optValue = allowable::IfAllowableGetValue(allowable::EndpointTypes, protocol); optValue) {
                options.protocol = *optValue;
                options.constructed.protocol = Network::ParseProtocol(*optValue);
            } else {
                std::cout << "Specified endpoint type is not allowed! Allowable types include: ";
                allowable::OutputValues(allowable::EndpointTypes);
                allowable = false;
            }
        }

        if (allowable) {
            std::string interface = "";
            std::cout << "Network Interface: (" << defaults::NetworkInterface << ") " << std::flush;
            std::getline(std::cin, interface); // Get the network interface that the node will be bound to.
            if (!interface.empty()) { options.interface = interface; }

            // Get the primary and secondary network address componentsCurrently, this may be the IP and port for TCP/IP
            // based nodes or frequency and channel for LoRa.
            std::string binding = "";
            std::string bindingOutputMessage = "Binding Address [IP:Port]: (";
            bindingOutputMessage.append(defaults::TcpBindingAddress.data());
            if (options.constructed.protocol == Network::Protocol::LoRa) {
                bindingOutputMessage = "Binding Frequency: [Frequency:Channel]: (";
                bindingOutputMessage.append(defaults::LoRaBindingAddress.data());
                options.binding = defaults::LoRaBindingAddress;
            }
            bindingOutputMessage.append(") ");

            std::cout << bindingOutputMessage << std::flush;
            std::getline(std::cin, binding);
            if (!binding.empty()) { options.binding = binding; }
 
            if (options.constructed.protocol != Network::Protocol::LoRa) {
                options.bootstrap = defaults::TcpBootstrapEntry;
                std::string bootstrap = "";
                std::cout << "Default Bootstrap Entry: (" << defaults::TcpBootstrapEntry << ") " << std::flush;
                std::getline(std::cin, bootstrap); // Get the default bootstrap entry for the protocol
                if (!bootstrap.empty()) { options.bootstrap = bootstrap; }
            }

            endpoints.emplace_back(std::move(options));
        }

        std::string sContinueChoice;
        std::cout << "Enter any key to setup a new endpoint configuration (Press enter to continue): " << std::flush;
        std::getline(std::cin, sContinueChoice);
        addEndpoint = !sContinueChoice.empty() || endpoints.empty();
        std::cout << "\n";
    } while(addEndpoint);

    return endpoints;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Security local::GetSecurityFromUser()
{
    Configuration::Options::Security options{ defaults::SecurityStrategy, defaults::NetworkToken };
    
    bool allowable = true; // Ensure the loop condition is initialized for the exit condition.
    do {
        // Get the desired security strategy from the user.
        allowable = true;
        std::string strategy{ defaults::SecurityStrategy };
        std::cout << "Security Strategy: (" << defaults::SecurityStrategy << ") " << std::flush;
        std::getline(std::cin, strategy);
        if (!strategy.empty()) {
            if (auto const optValue = allowable::IfAllowableGetValue(allowable::StrategyTypes, strategy); optValue) {
                options.strategy = strategy;
                options.constructed.strategy = Security::ConvertToStrategy(strategy);
            } else {
                std::cout << "Specified strategy is not allowed! Allowable types include: ";
                allowable::OutputValues(allowable::StrategyTypes);
                allowable = false;
            }

            if (allowable) {
                std::string token{ defaults::NetworkToken };
                std::cout << "Network Token: (" << defaults::NetworkToken << ") " << std::flush;
                std::getline(std::cin, token);
                if (!token.empty()) { options.token = token; }
            }
        }
        std::cout << "\n";
    } while(!allowable);

    return options;
}

//----------------------------------------------------------------------------------------------------------------------
