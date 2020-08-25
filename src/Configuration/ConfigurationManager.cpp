//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "ConfigurationManager.hpp"
#include "../BryptIdentifier/BryptIdentifier.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Utilities/FileUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
//-----------------------------------------------------------------------------------------------
#include "../Libraries/metajson/metajson.hh"
#include <array>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <boost/algorithm/string.hpp>
//-----------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

void SerializeVersion(std::ofstream& out);
void SerializeIdentifierOptions(
    Configuration::TIdentifierOptions const& options,
    std::ofstream& out);
void SerializeNodeOptions(
    Configuration::TDetailsOptions const& options,
    std::ofstream& out);
void SerializeEndpointConfigurations(
    Configuration::EndpointConfigurations const& configurations,
    std::ofstream& out);
void SerializeSecurityOptions(
    Configuration::TSecurityOptions const& options,
    std::ofstream& out);

Configuration::TIdentifierOptions GetIdentifierOptionsFromUser();
Configuration::TDetailsOptions GetDetailsOptionsFromUser();
Configuration::EndpointConfigurations GetEndpointConfigurationsFromUser();
Configuration::TSecurityOptions GetSecurityOptionsFromUser();

void InitializeIdentifierOptions(Configuration::TIdentifierOptions& options);
void InitializeEndpointConfigurations(Configuration::EndpointConfigurations& configurations);

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace defaults {
//------------------------------------------------------------------------------------------------

constexpr std::uint32_t FileSizeLimit = 12'000; // Limit the configuration files to 12KB

constexpr std::string_view IdentifierType = "Persistent";

constexpr std::string_view EndpointType = "TCP";
constexpr std::string_view NetworkInterface = "lo";
constexpr std::string_view TcpBindingAddress = "*:35216";
constexpr std::string_view TcpBootstrapEntry = "127.0.0.1:35216";
constexpr std::string_view LoRaBindingAddress = "915:71";

constexpr std::string_view EncryptionStandard = "AES-256-CTR";
constexpr std::string_view NetworkToken = "01234567890123456789012345678901";
constexpr std::string_view CentralAutority = "https://bridge.brypt.com";

//------------------------------------------------------------------------------------------------
} // default namespace
//------------------------------------------------------------------------------------------------
namespace allowable {
//------------------------------------------------------------------------------------------------

std::array<std::string_view, 2> IdentifierTypes = {
    "Ephemeral",
    "Persistent"
};

std::array<std::string_view, 4> EndpointTypes = {
    "Direct",
    "LoRa",
    "StreamBridge",
    "TCP"
};

template <typename ArrayType, std::size_t ArraySize>
std::optional<std::string> IfAllowableGetValue(
    std::array<ArrayType, ArraySize> const& values,
    std::string_view value)
{
    auto const found = std::find_if(
        values.begin(), values.end(),
        [&value] (std::string_view const& entry)
        {
            return (boost::iequals(value, entry));
        });

    if (found == values.end()) {
        return {};
    }

    return std::string(found->begin(), found->end());
}

template <typename ArrayType, std::size_t ArraySize>
void OutputValues(std::array<ArrayType, ArraySize> const& values)
{
    std::cout << "[";
    for (std::uint32_t idx = 0; idx < values.size(); ++idx) {
        std::cout << "\"" << values[idx] << "\"";
        if (idx != values.size() - 1) {
            std::cout << ", ";
        }
    }
    std::cout << "]" << std::endl;
}

//------------------------------------------------------------------------------------------------
} // default namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Symbol loading for JSON encoding. 
// Note: Only select portions of the Configuration::TSettings object will be encoded and decoded
// into the JSON configuration file. Meaning using metajson it is possible to omit sections of 
// the struct. However, initalization is needed to fill out the other parts of the configuration
// after decoding the file.
//------------------------------------------------------------------------------------------------
IOD_SYMBOL(version)
// "version": String

IOD_SYMBOL(identifier)
// "identifier": {
//     "value": Optional String,
//     "type": String,
// }

IOD_SYMBOL(details)
// "details": {
//     "name": String,
//     "description": String,
//     "location": String
// }

IOD_SYMBOL(endpoints)
// "endpoints": [{
//     "type": String,
//     "interface": String,
//     "binding": String,
//     "bootstrap": Optional String
// }]

IOD_SYMBOL(security)
// "security": {
//     "standard": String,
//     "token": String,
//     "authority": String
// }

IOD_SYMBOL(authority)
IOD_SYMBOL(binding)
IOD_SYMBOL(bootstrap)
IOD_SYMBOL(description)
IOD_SYMBOL(interface)
IOD_SYMBOL(location)
IOD_SYMBOL(name)
IOD_SYMBOL(standard)
IOD_SYMBOL(technology)
IOD_SYMBOL(token)
IOD_SYMBOL(type)
IOD_SYMBOL(value)

//------------------------------------------------------------------------------------------------

Configuration::CManager::CManager()
    : m_filepath()
    , m_settings()
    , m_validated(false)
{
    m_filepath = GetDefaultConfigurationFilepath();
    FileUtils::CreateFolderIfNoneExist(m_filepath);
}

//-----------------------------------------------------------------------------------------------

Configuration::CManager::CManager(std::string_view filepath)
    : m_filepath(filepath)
    , m_settings()
    , m_validated(false)
{
    // If the filepath does not have a filename, attach the default config.json
    if (!m_filepath.has_filename()) {
        m_filepath = m_filepath / DefaultConfigurationFilename;
    }

    // If the filepath does not have a parent path, get and attach the default brypt folder
    if (!m_filepath.has_parent_path()) {
        m_filepath = GetDefaultBryptFolder() / m_filepath;
    }
    
    FileUtils::CreateFolderIfNoneExist(m_filepath);
}

//-----------------------------------------------------------------------------------------------

Configuration::CManager::CManager(TSettings const& settings)
    : m_filepath()
    , m_settings(settings)
    , m_validated(false)
{
    m_filepath = GetDefaultConfigurationFilepath();
    FileUtils::CreateFolderIfNoneExist(m_filepath);
    ValidateSettings();
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::CManager::FetchSettings()
{
    StatusCode status;
    if (std::filesystem::exists(m_filepath)) {
        NodeUtils::printo(
            "Reading configuration file at: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
        status = DecodeConfigurationFile();
    } else {
        NodeUtils::printo(
            "No configuration file exists! Generating file at: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
        status = GenerateConfigurationFile();
    }

    if (status != StatusCode::Success) {
        return status;
    }

    InitializeSettings();

    return status;
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::CManager::Serialize()
{
    if (!m_validated) {
        return StatusCode::InputError;
    }

    if (m_filepath.empty()) {
        return StatusCode::FileError;
    }

    std::ofstream out(m_filepath, std::ofstream::out);
    if (out.fail()) {
        return StatusCode::FileError;
    }

    out << "{\n";

    local::SerializeVersion(out);
    local::SerializeIdentifierOptions(m_settings.identifier, out);
    local::SerializeNodeOptions(m_settings.details, out);
    local::SerializeEndpointConfigurations(m_settings.endpoints, out);
    local::SerializeSecurityOptions(m_settings.security, out);

    out << "}" << std::flush;

    out.close();

    return StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
// Description: Generates a new configuration file based on user command line arguements or
// from user input.
//-----------------------------------------------------------------------------------------------
Configuration::StatusCode Configuration::CManager::GenerateConfigurationFile()
{
    // If the configuration has not been provided to the Configuration Manager
    // generate a configuration object from user input
    if (m_settings.endpoints.empty()) {
        GetConfigurationOptionsFromUser();
    }

    ValidateSettings();

    StatusCode const status = Serialize();
    if (status != StatusCode::Success) {
        NodeUtils::printo(
            "Failed to save configuration settings to: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
    }

    return status;
}

//-----------------------------------------------------------------------------------------------

std::optional<Configuration::TSettings> Configuration::CManager::GetSettings() const
{
    if (!m_validated) {
        return {};
    }
    return m_settings;
}

//-----------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::CContainer> Configuration::CManager::GetBryptIdentifier() const
{
    if (!m_validated) {
        return {};
    }

    if (!m_settings.identifier.container.IsValid()) {
        return {};
    }

    return m_settings.identifier.container;
}

//-----------------------------------------------------------------------------------------------

std::string Configuration::CManager::GetNodeName() const
{
    if (!m_validated) {
        return {};
    }

    return m_settings.details.name;
}

//-----------------------------------------------------------------------------------------------

std::string Configuration::CManager::GetNodeDescription() const
{
    if (!m_validated) {
        return {};
    }

    return m_settings.details.description;
}

//-----------------------------------------------------------------------------------------------

std::string Configuration::CManager::GetNodeLocation() const
{
    if (!m_validated) {
        return {};
    }

    return m_settings.details.location;
}

//-----------------------------------------------------------------------------------------------
    
std::optional<Configuration::EndpointConfigurations> Configuration::CManager::GetEndpointConfigurations() const
{
    if (!m_validated) {
        return {};
    }
    return m_settings.endpoints;
}

//-----------------------------------------------------------------------------------------------

std::string Configuration::CManager::GetSecurityStandard() const
{
    if (!m_validated) {
        return {};
    }
    return m_settings.security.standard;
}

//-----------------------------------------------------------------------------------------------

std::string Configuration::CManager::GetCentralAuthority() const
{
    if (!m_validated) {
        return {};
    }
    return m_settings.security.authority;
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::CManager::ValidateSettings()
{
    m_validated = false;

    if (m_settings.identifier.type.empty()) {
        return StatusCode::DecodeError;
    } else {
        if (!allowable::IfAllowableGetValue(allowable::IdentifierTypes, m_settings.identifier.type)) {
            return StatusCode::DecodeError;
        }
    }

    if (m_settings.endpoints.empty()) {
        return StatusCode::DecodeError;
    } else {
        for (auto const& endpoint: m_settings.endpoints) {
            if (!allowable::IfAllowableGetValue(allowable::EndpointTypes, endpoint.technology)) {
                return StatusCode::DecodeError;
            }
        }
    }

    m_validated = true;
    return StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::CManager::DecodeConfigurationFile()
{
    // Determine the size of the file about to be read. Do not read a file
    // that is above the given treshold. 
    std::error_code error;
    auto const size = std::filesystem::file_size(m_filepath, error);
    if (error || size == 0 || size > defaults::FileSizeLimit) {
        return StatusCode::FileError;
    }

    // Attempt to open the configuration file, if it fails return noopt
    std::ifstream file(m_filepath);
    if (file.fail()) {
        return StatusCode::FileError;
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read the file into the buffer streamInitializeEndpointOptions
    std::string json = buffer.str(); // Put the file contents into a string to be trimmed and parsed
    // Remove newlines and tabs from the string
    json.erase(std::remove_if(json.begin(), json.end(), &FileUtils::IsNewlineOrTab), json.end());

    // Decode the JSON string into the configuration struct
    iod::json_object(
        s::version,
        s::identifier = iod::json_object(
            s::value,
            s::type),
        s::details = iod::json_object(
            s::name,
            s::description,
            s::location),
        s::endpoints = iod::json_vector(
            s::technology,
            s::interface,
            s::binding,
            s::bootstrap),
        s::security = iod::json_object(
            s::standard,
            s::token,
            s::authority)
        ).decode(json, m_settings);

    // Valdate the decoded settings. If the settings decoded are not valid return noopt.
    return ValidateSettings();
}

//-----------------------------------------------------------------------------------------------

void Configuration::CManager::GetConfigurationOptionsFromUser()
{
    std::cout << "Generating Brypt Node Configuration Settings." << std::endl; 
    std::cout << "Please Enter your Desired Network Options.\n" << std::endl;

    // Clear the cin stream 
    std::cin.clear(); std::cin.sync();

    TIdentifierOptions const identifierOptions = local::GetIdentifierOptionsFromUser();
    TDetailsOptions const detailsOptions = local::GetDetailsOptionsFromUser();
    EndpointConfigurations const endpointConfigurations = local::GetEndpointConfigurationsFromUser();
    Configuration::TSecurityOptions const securityOptions =  local::GetSecurityOptionsFromUser();

    m_settings.identifier = identifierOptions;
    m_settings.details = detailsOptions;
    m_settings.endpoints = endpointConfigurations;
    m_settings.security = securityOptions;
}

//-----------------------------------------------------------------------------------------------

void Configuration::CManager::InitializeSettings()
{
    local::InitializeIdentifierOptions(m_settings.identifier);
    local::InitializeEndpointConfigurations(m_settings.endpoints);

    // Update the configuration file as the initialization of options may create new values 
    // for certain options. Currently, this only caused by the generation of Brypt Identifiers.
    auto const status = Serialize();
    if (status != StatusCode::Success) {
        NodeUtils::printo(
            "Failed to update configuration file at: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
    }
}

//-----------------------------------------------------------------------------------------------

void local::SerializeVersion(std::ofstream& out)
{
    out << "\t\"version\": \"" << Brypt::Version << "\",\n";
}

//-----------------------------------------------------------------------------------------------

void local::SerializeIdentifierOptions(
    Configuration::TIdentifierOptions const& options,
    std::ofstream& out)
{
    out << "\t\"identifier\": {\n";
    if (options.value && options.type == "Persistent") {
        out << "\t\t\"value\": \"" << *options.value << "\",\n";
    }
    out << "\t\t\"type\": \"" << options.type << "\"\n";
    out << "\t},\n";
}

//-----------------------------------------------------------------------------------------------

void local::SerializeNodeOptions(
    Configuration::TDetailsOptions const& options,
    std::ofstream& out)
{
    out << "\t\"details\": {\n";
    out << "\t\t\"name\": \"" << options.name << "\",\n";
    out << "\t\t\"description\": \"" << options.description << "\",\n";
    out << "\t\t\"location\": \"" << options.location << "\"\n";
    out << "\t},\n";
}

//-----------------------------------------------------------------------------------------------

void local::SerializeEndpointConfigurations(
    Configuration::EndpointConfigurations const& configurations,
    std::ofstream& out)
{
    out << "\t\"endpoints\": [\n";
    for (auto const& options: configurations) {
        out << "\t\t{\n";
        out << "\t\t\t\"technology\": \"" << options.technology << "\",\n";
        out << "\t\t\t\"interface\": \"" << options.interface << "\",\n";
        out << "\t\t\t\"binding\": \"" << options.binding;
        if (options.bootstrap) {
            out << "\",\n\t\t\t\"bootstrap\": \"" << *options.bootstrap << "\"\n";
        } else {
            out << "\"\n";
        }
        out << "\t\t}";
        if (&options != &configurations.back()) {
            out << ",\n";
        }
    }
    out << "\n\t],\n";
}

//-----------------------------------------------------TDetailsOptions------------------------------------------

void local::SerializeSecurityOptions(
    Configuration::TSecurityOptions const& options,
    std::ofstream& out)
{
    out << "\t\"security\": {\n";
    out << "\t\t\"standard\": \"" << options.standard << "\",\n";
    out << "\t\t\"token\": \"" << options.token << "\",\n";
    out << "\t\t\"authority\": \"" << options.authority << "\"\n";
    out << "\t}\n";
}

//-----------------------------------------------------------------------------------------------

Configuration::TIdentifierOptions local::GetIdentifierOptionsFromUser()
{
    Configuration::TIdentifierOptions options(
        defaults::IdentifierType
    );

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

//-----------------------------------------------------------------------------------------------

Configuration::TDetailsOptions local::GetDetailsOptionsFromUser()
{
    Configuration::TDetailsOptions options;
    
    std::string name = "";
    std::cout << "Node Name: " << std::flush;
    std::getline(std::cin, name);
    if (!name.empty()) {
        options.name = name;
    }

    std::string decription = "";
    std::cout << "Node Description: " << std::flush;
    std::getline(std::cin, decription);
    if (!decription.empty()) {
        options.description = decription;
    }

    // TODO: Possibly expand on location to have different types. Possible types could include
    // worded description, geographic coordinates, custom map coordinates, etc.
    std::string location = "";
    std::cout << "Node Location: " << std::flush;
    std::getline(std::cin, location);
    if (!location.empty()) {
        options.location = location;
    }

    std::cout << "\n";

    return options;
}

//-----------------------------------------------------------------------------------------------

Configuration::EndpointConfigurations local::GetEndpointConfigurationsFromUser()
{
    Configuration::EndpointConfigurations configurations;

    bool bAddEndpointOption = false;
    do {
        Configuration::TEndpointOptions options(
            defaults::EndpointType,
            defaults::NetworkInterface,
            defaults::TcpBindingAddress);

        // Get the desired primary technology type for the node
        bool bAllowableEndpointType = true;
        std::string technology(defaults::EndpointType);
        std::cout << "EndpointType: (" << defaults::EndpointType << ") " << std::flush;
        std::getline(std::cin, technology);
        if (!technology.empty()) {
            if (auto const optValue = allowable::IfAllowableGetValue(allowable::EndpointTypes, technology); optValue) {
                options.technology = *optValue;
                options.type = Endpoints::ParseTechnologyType(*optValue);
            } else {
                std::cout << "Specified endpoint type is not allowed! Allowable types include: ";
                allowable::OutputValues(allowable::EndpointTypes);
                bAllowableEndpointType = false;
            }
        }

        if (bAllowableEndpointType) {
            // Get the network interface that the node will be bound too
            std::string interface(defaults::NetworkInterface);
            std::cout << "Network Interface: (" << defaults::NetworkInterface << ") " << std::flush;
            std::getline(std::cin, interface);
            if (!interface.empty()) {
                options.interface = interface;
            }

            // Get the primary and secondary network address components
            // Currently, this may be the IP and port for TCP/IP based nodes
            // or frequency and channel for LoRa.
            std::string binding(defaults::TcpBindingAddress);
            std::string bindingOutputMessage = "Binding Address [IP:Port]: (";
            bindingOutputMessage.append(defaults::TcpBindingAddress.data());
            if (options.type == Endpoints::TechnologyType::LoRa) {
                bindingOutputMessage = "Binding Frequency: [Frequency:Channel]: (";
                bindingOutputMessage.append(defaults::LoRaBindingAddress.data());
                options.binding = defaults::LoRaBindingAddress;
            }
            bindingOutputMessage.append(") ");

            std::cout << bindingOutputMessage << std::flush;
            std::getline(std::cin, binding);
            if (!binding.empty()) {
                options.binding = binding;
            }

            // Get the default bootstrap entry for the technology
            if (options.type != Endpoints::TechnologyType::LoRa) {
                std::string bootstrap(defaults::TcpBootstrapEntry);
                std::cout << "Default Bootstrap Entry: (" << defaults::TcpBootstrapEntry << ") " << std::flush;
                std::getline(std::cin, bootstrap);
                options.bootstrap = bootstrap;
            }

            configurations.emplace_back(options);
        }

        std::string sContinueChoice;
        std::cout << "Enter any key to setup a new endpoint configuration (Press enter to continue): " << std::flush;
        std::getline(std::cin, sContinueChoice);
        bAddEndpointOption = !sContinueChoice.empty();
        std::cout << "\n";
    } while(bAddEndpointOption);

    return configurations;
}

//-----------------------------------------------------------------------------------------------

Configuration::TSecurityOptions local::GetSecurityOptionsFromUser()
{
    Configuration::TSecurityOptions options(
        defaults::EncryptionStandard,
        defaults::NetworkToken,
        defaults::CentralAutority
    );
    
    std::string token = "";
    std::cout << "Network Token: (" << defaults::NetworkToken << ") " << std::flush;
    std::getline(std::cin, token);
    if (!token.empty()) {
        options.token = token;
    }

    std::string authority = "";
    std::cout << "Central Authority: (" << defaults::CentralAutority << ") " << std::flush;
    std::getline(std::cin, authority);
    if (!authority.empty()) {
        options.authority = authority;
    }

    std::cout << "\n";

    return options;
}

//-----------------------------------------------------------------------------------------------

void local::InitializeIdentifierOptions(Configuration::TIdentifierOptions& options)
{
    // If the identifier type is Ephemeral, then a new identifier should be generated. 
    // If the identifier type is Persistent, the we shall attempt to use provided value.
    // Would should never hit this function before the the options have been validated.
    if (options.type == "Ephemeral") {
        // Generate a new Brypt Identifier and store the network representation as the value.
        options.container = BryptIdentifier::CContainer(BryptIdentifier::Generate());
        options.value = options.container.GetNetworkRepresentation();
    } else if (options.type == "Persistent") {
        // If an identifier value has been parsed, attempt to use the provided value as
        // the identifier. We need to check the validity of the identifier to ensure the
        // value was properly formatted. 
        // Otherwise, a new Brypt Identifier must be be generated. 
        if (options.value) {
            options.container = BryptIdentifier::CContainer(*options.value);
            if (!options.container.IsValid()) {
                options.value.reset();
            }
        } else {
            options.container = BryptIdentifier::CContainer(BryptIdentifier::Generate());
            options.value = options.container.GetNetworkRepresentation();  
        }
    } else {
        assert(true); // How did these identifier options pass validation?
    }
}

//-----------------------------------------------------------------------------------------------

void  local::InitializeEndpointConfigurations(Configuration::EndpointConfigurations& configurations)
{
    std::for_each(configurations.begin(), configurations.end(),
    [] (auto& endpoint)
        {
            endpoint.type = Endpoints::ParseTechnologyType(endpoint.technology);
        }
    );
}

//-----------------------------------------------------------------------------------------------
