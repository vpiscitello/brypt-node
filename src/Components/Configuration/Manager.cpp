//----------------------------------------------------------------------------------------------------------------------
// File: Manager.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Manager.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/LogUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/algorithm/string.hpp>
#include <lithium_json.hh>
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

void SerializeVersion(std::ofstream& out);
void SerializeIdentifierOptions(Configuration::IdentifierOptions const& options, std::ofstream& out);
void SerializeNodeOptions(Configuration::DetailsOptions const& options, std::ofstream& out);
void SerializeEndpointOptions(Configuration::EndpointsSet const& configurations, std::ofstream& out);
void SerializeSecurityOptions(Configuration::SecurityOptions const& options, std::ofstream& out);

Configuration::IdentifierOptions GetIdentifierOptionsFromUser();
Configuration::DetailsOptions GetDetailsOptionsFromUser();
Configuration::EndpointsSet GetEndpointOptionsFromUser();
Configuration::SecurityOptions GetSecurityOptionsFromUser();

[[nodiscard]] bool InitializeIdentifierOptions(Configuration::IdentifierOptions& options);
[[nodiscard]] bool InitializeEndpointOptions(
    Configuration::EndpointsSet& endpoints, std::shared_ptr<spdlog::logger> const& spLogger);
void InitializeSecurityOptions(Configuration::SecurityOptions& options);

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
constexpr std::string_view CentralAutority = "https://bridge.brypt.com";

//----------------------------------------------------------------------------------------------------------------------
} // default namespace
//----------------------------------------------------------------------------------------------------------------------
namespace allowable {
//----------------------------------------------------------------------------------------------------------------------

std::array<std::string_view, 2> IdentifierTypes = { "Ephemeral", "Persistent" };
std::array<std::string_view, 2> EndpointTypes = { "LoRa", "TCP" };
std::array<std::string_view, 1> StrategyTypes = { "PQNISTL3" };

template <typename ArrayType, std::size_t ArraySize>
std::optional<std::string> IfAllowableGetValue(
    std::array<ArrayType, ArraySize> const& values,
    std::string_view value)
{
    auto const found = std::find_if(
        values.begin(), values.end(),
        [&value] (std::string_view const& entry) { return (boost::iequals(value, entry));  });

    if (found == values.end()) {  return {}; }

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
//     "token": String,
//     "authority": String
// }

#ifndef LI_SYMBOL_authority
#define LI_SYMBOL_authority
LI_SYMBOL(authority)
#endif
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

Configuration::Manager::Manager(
    std::filesystem::path const& filepath, bool isGeneratorAllowed, bool shouldBuildPath)
    : m_spLogger(spdlog::get(LogUtils::Name::Core.data()))
    , m_isGeneratorAllowed(isGeneratorAllowed)
    , m_filepath(filepath)
    , m_settings()
    , m_validated(false)
{
    assert(m_spLogger);

    if (!shouldBuildPath) { return; }

    // If the filepath does not have a filename, attach the default config.json
    if (!m_filepath.has_filename()) { m_filepath = m_filepath / DefaultConfigurationFilename; }

    // If the filepath does not have a parent path, get and attach the default brypt folder
    if (!m_filepath.has_parent_path()) {  m_filepath = GetDefaultBryptFolder() / m_filepath; }
    
    if (m_isGeneratorAllowed && !FileUtils::CreateFolderIfNoneExist(m_filepath)) {
        m_spLogger->error("Failed to create the filepath at: {}!", m_filepath.string());
    }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Manager::Manager(Settings const& settings)
    : m_spLogger(spdlog::get(LogUtils::Name::Core.data()))
    , m_isGeneratorAllowed(false)
    , m_filepath()
    , m_settings(settings)
    , m_validated(false)
{
    assert(m_spLogger);
    m_filepath = GetDefaultConfigurationFilepath();
    if (!FileUtils::CreateFolderIfNoneExist(m_filepath)) {
        m_spLogger->error("Failed to create the filepath at: {}!", m_filepath.string());
        return;
    }
    ValidateSettings();
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Manager::FetchSettings()
{
    StatusCode status;
    if (std::filesystem::exists(m_filepath)) {
        m_spLogger->debug("Reading configuration file at: {}.", m_filepath.string());
        status = DecodeConfigurationFile();
    } else {
        if (!m_isGeneratorAllowed) {
            std::ostringstream oss;
            oss << "Unable to locate: {}; The configuration generator could not be launched ";
            oss << "with \"--non-interactive\" enabled.";
            m_spLogger->error(oss.str(), m_filepath.string());
            return StatusCode::FileError;
        }

        m_spLogger->warn(
            "A configuration file could not be found. Launching configuration generator for: {}.",
            m_filepath.string());
        status = GenerateConfigurationFile();
    }

    if (status != StatusCode::Success) { return status; }

    if (!InitializeSettings()) { return StatusCode::InputError; }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Manager::Serialize()
{
    if (!m_validated) { return StatusCode::InputError; }
    if (m_filepath.empty()) { return StatusCode::FileError; }

    std::ofstream out(m_filepath, std::ofstream::out);
    if (out.fail()) { return StatusCode::FileError; }

    out << "{\n";

    local::SerializeVersion(out);
    local::SerializeIdentifierOptions(m_settings.identifier, out);
    local::SerializeNodeOptions(m_settings.details, out);
    local::SerializeEndpointOptions(m_settings.endpoints, out);
    local::SerializeSecurityOptions(m_settings.security, out);

    out << "}" << std::flush;

    out.close();

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Generates a new configuration file based on user handler line arguements or
// from user input.
//----------------------------------------------------------------------------------------------------------------------
Configuration::StatusCode Configuration::Manager::GenerateConfigurationFile()
{
    // If the configuration has not been provided to the Configuration Manager
    // generate a configuration object from user input
    if (m_settings.endpoints.empty()) { GetSettingsFromUser(); }

    ValidateSettings();

    StatusCode const status = Serialize();
    if (status != StatusCode::Success) {
        m_spLogger->error("Failed to save configuration settings to: {}!", m_filepath.string());
    }

    return status;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Manager::IsValidated() const
{
    return m_validated;
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Configuration::Manager::GetNodeIdentifier() const
{
    assert(m_validated);
    assert(m_settings.identifier.container && m_settings.identifier.container->IsValid());
    return m_settings.identifier.container;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Manager::GetNodeName() const
{
    assert(m_validated);
    return m_settings.details.name;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Manager::GetNodeDescription() const
{
    assert(m_validated);
    return m_settings.details.description;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Manager::GetNodeLocation() const
{
    assert(m_validated);
    return m_settings.details.location;
}

//----------------------------------------------------------------------------------------------------------------------
    
Configuration::EndpointsSet const& Configuration::Manager::GetEndpointOptions() const
{
    assert(m_validated);
    assert(m_settings.endpoints.size() != 0);
    return m_settings.endpoints;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Strategy Configuration::Manager::GetSecurityStrategy() const
{
    assert(m_validated);
    assert(m_settings.security.type != Security::Strategy::Invalid);
    return m_settings.security.type;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Manager::GetCentralAuthority() const
{
    assert(m_validated);
    return m_settings.security.authority;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Manager::ValidateSettings()
{
    m_validated = false;

    if (m_settings.identifier.type.empty()) { return StatusCode::DecodeError; }
    if (!allowable::IfAllowableGetValue(allowable::IdentifierTypes, m_settings.identifier.type)) {
        return StatusCode::DecodeError;
    }

    if (m_settings.endpoints.empty()) { return StatusCode::DecodeError; }
    for (auto const& endpoint: m_settings.endpoints) {
        if (!allowable::IfAllowableGetValue(allowable::EndpointTypes, endpoint.protocol)) {
            return StatusCode::DecodeError;
        }
    }

    if (!allowable::IfAllowableGetValue(allowable::StrategyTypes, m_settings.security.strategy)) {
        return StatusCode::DecodeError;
    }

    m_validated = true;

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Manager::DecodeConfigurationFile()
{
    // Determine the size of the file about to be read. Do not read a file
    // that is above the given treshold. 
    std::error_code error;
    auto const size = std::filesystem::file_size(m_filepath, error);
    if (error || size == 0 || size > defaults::FileSizeLimit) { return StatusCode::FileError; }

    // Attempt to open the configuration file, if it fails return noopt
    std::ifstream file(m_filepath);
    if (file.fail()) { return StatusCode::FileError; }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read the file into the buffer stream
    std::string json = buffer.str(); // Put the file contents into a string to be trimmed and parsed
    // Remove newlines and tabs from the string
    json.erase(std::remove_if(json.begin(), json.end(), &FileUtils::IsNewlineOrTab), json.end());

    // Decode the JSON string into the configuration struct
    li::json_object(
        s::version,
        s::identifier = li::json_object(
            s::type,
            s::value),
        s::details = li::json_object(
            s::name,
            s::description,
            s::location),
        s::endpoints = li::json_vector(
            s::protocol,
            s::interface,
            s::binding,
            s::bootstrap),
        s::security = li::json_object(
            s::strategy,
            s::token,
            s::authority)
        ).decode(json, m_settings);

    // Valdate the decoded settings. If the settings decoded are not valid return noopt.
    return ValidateSettings();
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Manager::GetSettingsFromUser()
{
    std::cout << "Generating Brypt Node Configuration Settings." << std::endl; 
    std::cout << "Please Enter your Desired Network Options.\n" << std::endl;

    // Clear the cin stream 
    std::cin.clear(); std::cin.sync();

    m_settings.identifier = local::GetIdentifierOptionsFromUser();
    m_settings.details = local::GetDetailsOptionsFromUser();
    m_settings.endpoints = local::GetEndpointOptionsFromUser();
    m_settings.security = local::GetSecurityOptionsFromUser();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Manager::InitializeSettings()
{
    if (!local::InitializeIdentifierOptions(m_settings.identifier)) { return false; }
    if (!local::InitializeEndpointOptions(m_settings.endpoints, m_spLogger)) { return false; }

    local::InitializeSecurityOptions(m_settings.security);

    // Update the configuration file as the initialization of options may create new values 
    // for certain options. Currently, this only caused by the generation of Brypt Identifiers.
    auto const status = Serialize();
    if (status != StatusCode::Success) {
        m_spLogger->error("Failed to update configuration file at: {}!", m_filepath.string());
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void local::SerializeVersion(std::ofstream& out)
{
    out << "\t\"version\": \"" << Brypt::Version << "\",\n";
}

//----------------------------------------------------------------------------------------------------------------------

void local::SerializeIdentifierOptions(Configuration::IdentifierOptions const& options, std::ofstream& out)
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

void local::SerializeNodeOptions(Configuration::DetailsOptions const& options, std::ofstream& out)
{
    out << "\t\"details\": {\n";
    out << "\t\t\"name\": \"" << options.name << "\",\n";
    out << "\t\t\"description\": \"" << options.description << "\",\n";
    out << "\t\t\"location\": \"" << options.location << "\"\n";
    out << "\t},\n";
}

//----------------------------------------------------------------------------------------------------------------------

void local::SerializeEndpointOptions(Configuration::EndpointsSet const& endpoints, std::ofstream& out)
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

void local::SerializeSecurityOptions(Configuration::SecurityOptions const& options, std::ofstream& out)
{
    out << "\t\"security\": {\n";
    out << "\t\t\"strategy\": \"" << options.strategy << "\",\n";
    out << "\t\t\"token\": \"" << options.token << "\",\n";
    out << "\t\t\"authority\": \"" << options.authority << "\"\n";
    out << "\t}\n";
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::IdentifierOptions local::GetIdentifierOptionsFromUser()
{
    Configuration::IdentifierOptions options(defaults::IdentifierType);

    // Get the network interface that the node will be bound too
    std::string type(defaults::IdentifierType);
    std::cout << "Identifier Type: (" << defaults::IdentifierType << ") " << std::flush;
    std::getline(std::cin, type);
    if (!type.empty()) {
        if (auto const optValue = allowable::IfAllowableGetValue(allowable::IdentifierTypes, type); optValue) {
            options.type = *optValue;
        } else {
            std::cout << "Specified identifier type is not allowed! Allowable types include: ";
            allowable::OutputValues(allowable::IdentifierTypes);
            std::cout << std::endl;
            return local::GetIdentifierOptionsFromUser();
        }
    }
    std::cout << std::endl;

    return options;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DetailsOptions local::GetDetailsOptionsFromUser()
{
    Configuration::DetailsOptions options;
    
    std::string name = "";
    std::cout << "Node Name: " << std::flush;
    std::getline(std::cin, name);
    if (!name.empty()) { options.name = name; }

    std::string decription = "";
    std::cout << "Node Description: " << std::flush;
    std::getline(std::cin, decription);
    if (!decription.empty()) { options.description = decription; }

    // TODO: Possibly expand on location to have different types. Possible types could include
    // worded description, geographic coordinates, custom map coordinates, etc.
    std::string location = "";
    std::cout << "Node Location: " << std::flush;
    std::getline(std::cin, location);
    if (!location.empty()) { options.location = location; }

    std::cout << "\n";

    return options;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::EndpointsSet local::GetEndpointOptionsFromUser()
{
    Configuration::EndpointsSet endpoints;

    bool bAddEndpointOption = false;
    do {
        Configuration::EndpointOptions options(
            defaults::EndpointType,
            defaults::NetworkInterface,
            defaults::TcpBindingAddress);

        // Get the desired primary protocol type for the node
        bool bAllowableEndpointType = true;
        std::string protocol = "";
        std::cout << "EndpointType: (" << defaults::EndpointType << ") " << std::flush;
        std::getline(std::cin, protocol);
        if (!protocol.empty()) {
            if (auto const optValue = allowable::IfAllowableGetValue(allowable::EndpointTypes, protocol); optValue) {
                options.protocol = *optValue;
                options.type = Network::ParseProtocol(*optValue);
            } else {
                std::cout << "Specified endpoint type is not allowed! Allowable types include: ";
                allowable::OutputValues(allowable::EndpointTypes);
                bAllowableEndpointType = false;
            }
        }

        if (bAllowableEndpointType) {
            // Get the network interface that the node will be bound too
            std::string interface = "";
            std::cout << "Network Interface: (" << defaults::NetworkInterface << ") " << std::flush;
            std::getline(std::cin, interface);
            if (!interface.empty()) { options.interface = interface; }

            // Get the primary and secondary network address components
            // Currently, this may be the IP and port for TCP/IP based nodes
            // or frequency and channel for LoRa.
            std::string binding = "";
            std::string bindingOutputMessage = "Binding Address [IP:Port]: (";
            bindingOutputMessage.append(defaults::TcpBindingAddress.data());
            if (options.type == Network::Protocol::LoRa) {
                bindingOutputMessage = "Binding Frequency: [Frequency:Channel]: (";
                bindingOutputMessage.append(defaults::LoRaBindingAddress.data());
                options.binding = defaults::LoRaBindingAddress;
            }
            bindingOutputMessage.append(") ");

            std::cout << bindingOutputMessage << std::flush;
            std::getline(std::cin, binding);
            if (!binding.empty()) { options.binding = binding; }

            // Get the default bootstrap entry for the protocol
            if (options.type != Network::Protocol::LoRa) {
                options.bootstrap = defaults::TcpBootstrapEntry;

                std::string bootstrap = "";
                std::cout << "Default Bootstrap Entry: (" << defaults::TcpBootstrapEntry << ") " << std::flush;
                std::getline(std::cin, bootstrap);
                if (!bootstrap.empty()) { options.bootstrap = bootstrap; }
            }

            endpoints.emplace_back(std::move(options));
        }

        std::string sContinueChoice;
        std::cout << "Enter any key to setup a new endpoint configuration (Press enter to continue): " << std::flush;
        std::getline(std::cin, sContinueChoice);
        bAddEndpointOption = !sContinueChoice.empty();
        std::cout << "\n";
    } while(bAddEndpointOption);

    return endpoints;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SecurityOptions local::GetSecurityOptionsFromUser()
{
    Configuration::SecurityOptions options(
        defaults::SecurityStrategy, defaults::NetworkToken, defaults::CentralAutority);
    
    bool bAllowableStrategyType;
    do {
        // Ensure the loop condition is initialized for the exit condition.
        bAllowableStrategyType = true;

        // Get the desired security strategy from the user.
        std::string strategy(defaults::SecurityStrategy);
        std::cout << "Security Strategy: (" << defaults::SecurityStrategy << ") " << std::flush;
        std::getline(std::cin, strategy);
        if (!strategy.empty()) {
            if (auto const optValue = allowable::IfAllowableGetValue(allowable::StrategyTypes, strategy); optValue) {
                options.strategy = strategy;
                options.type = Security::ConvertToStrategy(strategy);
            } else {
                std::cout << "Specified strategy is not allowed! Allowable types include: ";
                allowable::OutputValues(allowable::StrategyTypes);
                bAllowableStrategyType = false;
            }
        }

        if (bAllowableStrategyType) {
            std::string token(defaults::NetworkToken);
            std::cout << "Network Token: (" << defaults::NetworkToken << ") " << std::flush;
            std::getline(std::cin, token);
            if (!token.empty()) { options.token = token; }

            std::string authority(defaults::CentralAutority);
            std::cout << "Central Authority: (" << defaults::CentralAutority << ") " << std::flush;
            std::getline(std::cin, authority);
            if (!authority.empty()) { options.authority = authority; }
        }

        std::cout << "\n";
    } while(!bAllowableStrategyType);

    return options;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::InitializeIdentifierOptions(Configuration::IdentifierOptions& options)
{
    // If the identifier type is Ephemeral, then a new identifier should be generated. 
    // If the identifier type is Persistent, the we shall attempt to use provided value.
    // Would should never hit this function before the the options have been validated.
    if (options.type == "Ephemeral") {
        // Generate a new Brypt Identifier and store the network representation as the value.
        options.container = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
        assert(options.container);
        options.value = options.container->GetNetworkString();
    } else if (options.type == "Persistent") {
        // If an identifier value has been parsed, attempt to use the provided value as
        // the identifier. We need to check the validity of the identifier to ensure the
        // value was properly formatted. 
        // Otherwise, a new Brypt Identifier must be be generated. 
        if (options.value) {
            options.container = std::make_shared<Node::Identifier const>(*options.value);
            assert(options.container);
            if (!options.container->IsValid()) {
                options.value.reset();
                return false;
            }
        } else {
            options.container = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
            assert(options.container);
            options.value = options.container->GetNetworkString();  
        }
    } else {
        assert(false); // How did these identifier options pass validation?
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::InitializeEndpointOptions(
    Configuration::EndpointsSet& endpoints, std::shared_ptr<spdlog::logger> const& spLogger)
{
    bool success = true;
    std::for_each(endpoints.begin(), endpoints.end(),
        [&success, &spLogger] (auto& options)
        {
            if (!options.Initialize()) {
                spLogger->warn("Unable to initialize the endpoint configuration for {}", options.GetProtocolName());
                success = false;
            }
        });
    return success;
}

//----------------------------------------------------------------------------------------------------------------------

void local::InitializeSecurityOptions(Configuration::SecurityOptions& options)
{
    options.type = Security::ConvertToStrategy(options.strategy);
}

//----------------------------------------------------------------------------------------------------------------------
