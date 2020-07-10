//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "ConfigurationManager.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Utilities/FileUtils.hpp"
//-----------------------------------------------------------------------------------------------
#include "../Libraries/metajson/metajson.hh"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
//-----------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

void SerializeNodeOptions(
    Configuration::TDetailsOptions const& options,
    std::ofstream& out);
void SerializeEndpointConfigurations(
    Configuration::EndpointConfigurations const& configurations,
    std::ofstream& out);
void SerializeSecurityOptions(
    Configuration::TSecurityOptions const& options,
    std::ofstream& out);

Configuration::TDetailsOptions GetDetailsOptionsFromUser();
Configuration::EndpointConfigurations GetEndpointConfigurationsFromUser();
Configuration::TSecurityOptions GetSecurityOptionsFromUser();

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace defaults {
//------------------------------------------------------------------------------------------------

constexpr std::uint32_t FileSizeLimit = 12'000; // Limit the configuration files to 12KB

constexpr std::string_view TechnologyName = "Direct";
constexpr std::string_view NetworkInterface = "lo";
constexpr std::string_view TcpBindingAddress = "*:35216";
constexpr std::string_view LoRaBindingAddress = "915:71";

constexpr std::string_view EncryptionStandard = "AES-256-CTR";
constexpr std::string_view NetworkToken = "01234567890123456789012345678901";
constexpr std::string_view CentralAutority = "https://bridge.brypt.com:8080";

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
IOD_SYMBOL(details)
// "details": {
//     "version": String,
//     "name": String,
//     "description": String,
//     "location": String
// }

IOD_SYMBOL(endpoints)
// "endpoints": [{
//     "technology_name": String,
//     "interface": String,
//     "binding": String
// }]

IOD_SYMBOL(security)
// "security": {
//     "standard": String,
//     "token": String,
//     "central_authority": String
// }

IOD_SYMBOL(binding)
IOD_SYMBOL(central_authority)
IOD_SYMBOL(description)
IOD_SYMBOL(interface)
IOD_SYMBOL(location)
IOD_SYMBOL(name)
IOD_SYMBOL(standard)
IOD_SYMBOL(technology_name)
IOD_SYMBOL(token)
IOD_SYMBOL(version)

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

std::optional<Configuration::TSettings> Configuration::CManager::FetchSettings()
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
        return {};
    }

    InitializeSettings();

    return m_settings;
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

    local::SerializeNodeOptions(m_settings.details, out);
    local::SerializeEndpointConfigurations(m_settings.endpoints, out);
    local::SerializeSecurityOptions(m_settings.security, out);

    out << "}" << std::flush;

    out.close();

    return StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::CManager::ValidateSettings()
{
    m_validated = false;

    if (m_settings.endpoints.empty()) {
        return StatusCode::DecodeError;
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
    buffer << file.rdbuf(); // Read the file into the buffer stream
    std::string json = buffer.str(); // Put the file contents into a string to be trimmed and parsed
    // Remove newlines and tabs from the string
    json.erase(std::remove_if(json.begin(), json.end(), &FileUtils::IsNewlineOrTab), json.end());

    // Decode the JSON string into the configuration struct
    iod::json_object(
        s::details = iod::json_object(
            s::version,
            s::name,
            s::description,
            s::location),
        s::endpoints = iod::json_vector(
            s::technology_name,
            s::interface,
            s::binding),
        s::security = iod::json_object(
            s::standard,
            s::token,
            s::central_authority)
        ).decode(json, m_settings);

    // Valdate the decoded settings. If the settings decoded are not valid return noopt.
    return ValidateSettings();
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

std::optional<Configuration::TSettings> Configuration::CManager::GetCachedConfiguration() const
{
    if (!m_validated) {
        return {};
    }
    return m_settings;
}

//-----------------------------------------------------------------------------------------------

void Configuration::CManager::GetConfigurationOptionsFromUser()
{
    std::cout << "Generating Brypt Node Configuration Settings." << std::endl; 
    std::cout << "Please Enter your Desired Network Options.\n" << std::endl;

    // Clear the cin stream 
    std::cin.clear(); std::cin.sync();

    TDetailsOptions const detailsOptions = 
        local::GetDetailsOptionsFromUser();

    EndpointConfigurations const endpointConfigurations = 
        local::GetEndpointConfigurationsFromUser();

    Configuration::TSecurityOptions const securityOptions = 
        local::GetSecurityOptionsFromUser();

    m_settings.details = detailsOptions;
    m_settings.endpoints = endpointConfigurations;
    m_settings.security = securityOptions;
}

//-----------------------------------------------------------------------------------------------

void Configuration::CManager::InitializeSettings()
{
    std::for_each(m_settings.endpoints.begin(), m_settings.endpoints.end(),
    [] (auto& endpoint)
        {
            endpoint.technology = Endpoints::ParseTechnologyType(endpoint.technology_name);
        }
    );
}

//-----------------------------------------------------------------------------------------------

void local::SerializeNodeOptions(
    Configuration::TDetailsOptions const& options,
    std::ofstream& out)
{
    out << "\t\"details\": {\n";
    out << "\t\t\"version\": \"" << options.version << "\",\n";
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
        out << "\t\t\t\"technology_name\": \"" << Endpoints::TechnologyTypeToString(options.technology) << "\",\n";
        out << "\t\t\t\"interface\": \"" << options.interface << "\",\n";
        out << "\t\t\t\"binding\": \"" << options.binding << "\"\n";
        out << "\t\t}";
        if (&options != &configurations.back()) {
            out << ",\n";
        }
    }
    out << "\n\t],\n";
}

//-----------------------------------------------------------------------------------------------

void local::SerializeSecurityOptions(
    Configuration::TSecurityOptions const& options,
    std::ofstream& out)
{
    out << "\t\"security\": {\n";
    out << "\t\t\"standard\": \"" << options.standard << "\",\n";
    out << "\t\t\"token\": \"" << options.token << "\",\n";
    out << "\t\t\"central_authority\": \"" << options.central_authority << "\"\n";
    out << "\t}\n";
}

//-----------------------------------------------------------------------------------------------

Configuration::TDetailsOptions local::GetDetailsOptionsFromUser()
{
    Configuration::TDetailsOptions options;
    
    std::string sName = "";
    std::cout << "Node Name: " << std::flush;
    std::getline(std::cin, sName);
    if (!sName.empty()) {
        options.name = sName;
    }

    std::string sDecription = "";
    std::cout << "Node Description: " << std::flush;
    std::getline(std::cin, sDecription);
    if (!sDecription.empty()) {
        options.description = sDecription;
    }

    // TODO: Possibly expand on location to have different types. Possible types could include
    // worded description, geographic coordinates, custom map coordinates, etc.
    std::string sLocation = "";
    std::cout << "Node Location: " << std::flush;
    std::getline(std::cin, sLocation);
    if (!sLocation.empty()) {
        options.location = sLocation;
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
            defaults::TechnologyName,
            defaults::NetworkInterface,
            defaults::TcpBindingAddress);

        // Get the desired primary technology type for the node
        std::string sTechnology = "";
        std::cout << "Communication Technology: (" << defaults::TechnologyName << ") " << std::flush;
        std::getline(std::cin, sTechnology);
        if (!sTechnology.empty()) {
            options.technology_name = sTechnology;
            options.technology = Endpoints::ParseTechnologyType(sTechnology);
        }

        // Get the network interface that the node will be bound too
        std::string sInterface = "";
        std::cout << "Network Interface: (" << defaults::NetworkInterface << ") " << std::flush;
        std::getline(std::cin, sInterface);
        if (!sInterface.empty()) {
            options.interface = sInterface;
        }

        // Get the primary and secondary network address components
        // Currently, this may be the IP and port for TCP/IP based nodes
        // or frequency and channel for LoRa.
        std::string sBinding = "";
        std::string bindingOutputMessage = "Binding Address [IP:Port]: (";
        bindingOutputMessage.append(defaults::TcpBindingAddress.data());
        if (options.technology == Endpoints::TechnologyType::LoRa) {
            bindingOutputMessage = "Binding Frequency: [Frequency:Channel]: (";
            bindingOutputMessage.append(defaults::LoRaBindingAddress.data());
            options.binding = defaults::LoRaBindingAddress;
        }
        bindingOutputMessage.append(") ");
        std::cout << bindingOutputMessage << std::flush;
        std::getline(std::cin, sBinding);
        if (!sBinding.empty()) {
            options.binding = sBinding;
        }

        configurations.emplace_back(options);

        std::string sContinueChoice;
        std::cout << "Enter any key to setup a key (Press enter to continue): " << std::flush;
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
    
    std::string sToken = "";
    std::cout << "Network Token: (" << defaults::NetworkToken << ") " << std::flush;
    std::getline(std::cin, sToken);
    if (!sToken.empty()) {
        options.token = sToken;
    }

    std::string sCentralAuthority = "";
    std::cout << "Central Authority: (" << defaults::CentralAutority << ") " << std::flush;
    std::getline(std::cin, sCentralAuthority);
    if (!sCentralAuthority.empty()) {
        options.central_authority = sCentralAuthority;
    }

    std::cout << "\n";

    return options;
}

//-----------------------------------------------------------------------------------------------
