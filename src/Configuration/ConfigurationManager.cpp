//------------------------------------------------------------------------------------------------
// File: ConfigurationManager.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Configuration.hpp"
#include "ConfigurationManager.hpp"
//-----------------------------------------------------------------------------------------------
#include "../Libraries/metajson/metajson.hh"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
//-----------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

std::string const FallbackConfigPath = "/etc/";
std::string const BryptConfigFolder = "/brypt/";

constexpr std::uint32_t FileSizeLimit = 12'000; // Limit the configuration files that read to 12KB

bool IsNewlineOrTab(char c);

void WriteNodeOptions(
    Configuration::TDetailsOptions const& options,
    std::ofstream& out);
void WriteEndpointOptions(
    std::vector<Configuration::TEndpointOptions> const& options,
    std::ofstream& out);
void WriteSecurityOptions(
    Configuration::TSecurityOptions const& options,
    std::ofstream& out);

Configuration::TDetailsOptions GetDetailsOptionsFromUser();
std::vector<Configuration::TEndpointOptions> GetEndpointOptionsFromUser();
Configuration::TSecurityOptions GetSecurityOptionsFromUser();

//------------------------------------------------------------------------------------------------
} // local namespace
//------------------------------------------------------------------------------------------------
namespace defaults {
//------------------------------------------------------------------------------------------------

constexpr std::string_view TechnologyName = "Direct";
constexpr std::string_view NetworkInterface = "lo";
constexpr std::string_view TcpBindingAddress = "*:3005";
constexpr std::string_view LoRaBindingAddress = "861.1:0";
constexpr std::string_view EntryAddress = "127.0.0.1:9999";

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
IOD_SYMBOL(version)
IOD_SYMBOL(name)
IOD_SYMBOL(description)
IOD_SYMBOL(location)

IOD_SYMBOL(endpoints)
IOD_SYMBOL(technology_name)
IOD_SYMBOL(interface)
IOD_SYMBOL(binding)
IOD_SYMBOL(entry_address)

IOD_SYMBOL(security)
IOD_SYMBOL(standard)
IOD_SYMBOL(token)
IOD_SYMBOL(central_authority)
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

Configuration::CManager::CManager()
    : m_filepath()
    , m_settings()
    , m_validated(false)
{
    m_filepath = GetDefaultConfigurationPath();
}

//-----------------------------------------------------------------------------------------------

Configuration::CManager::CManager(std::string const& filepath)
    : m_filepath(filepath)
    , m_settings()
    , m_validated(false)
{
}

//-----------------------------------------------------------------------------------------------

std::optional<Configuration::TSettings> Configuration::CManager::Parse()
{
    CreateConfigurationFilePath();

    StatusCode status;
    if (std::filesystem::exists(m_filepath)) {
        NodeUtils::printo(
            "Reading configuration file at: " + m_filepath.string() + "\n",
            NodeUtils::PrintType::Node);
        status = DecodeConfigurationFile();
    } else {
        NodeUtils::printo(
            "No configuration file exists! Generating file at: " + m_filepath.string() + "\n",
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

Configuration::CManager::StatusCode Configuration::CManager::Save()
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

    local::WriteNodeOptions(m_settings.details, out);
    local::WriteEndpointOptions(m_settings.endpoints, out);
    local::WriteSecurityOptions(m_settings.security, out);

    out << "}" << std::flush;

    out.close();

    return StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

Configuration::CManager::StatusCode Configuration::CManager::ValidateSettings()
{
    m_validated = false;

    if (m_settings.endpoints.empty()) {
        return StatusCode::DecodeError;
    }

    if (m_settings.security.token.empty()) {
        return StatusCode::DecodeError;
    }

    m_validated = true;
    return StatusCode::Success;
}

//-----------------------------------------------------------------------------------------------

Configuration::CManager::StatusCode Configuration::CManager::DecodeConfigurationFile()
{
    // Determine the size of the file about to be read. Do not read a file
    // that is above the given treshold. 
    std::error_code error;
    auto const size = std::filesystem::file_size(m_filepath, error);
    if (error || size == 0 || size > local::FileSizeLimit) {
        return StatusCode::FileError;
    }

    // Attempt to open the configuration file, if it fails return noopt
    std::ifstream file(m_filepath);
    if (file.fail()) {
        return StatusCode::FileError;
    }

    std::stringstream buffer;
    buffer << file.rdbuf(); // Read the file into the buffer stream
    std::string jsonStr = buffer.str(); // Put the file contents into a string to be trimmed and parsed
    // Remove newlines and tabs from the string
    jsonStr.erase(std::remove_if(jsonStr.begin(), jsonStr.end(), &local::IsNewlineOrTab), jsonStr.end());

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
            s::binding,
            s::entry_address),
        s::security = iod::json_object(
            s::standard,
            s::token,
            s::central_authority)
        ).decode(jsonStr, m_settings);

    // Valdate the decoded settings. If the settings decoded are not valid return noopt.
    return ValidateSettings();
}

//-----------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------
// Description: Generates a new configuration file based on user command line arguements or
// from user input.
//-----------------------------------------------------------------------------------------------
Configuration::CManager::StatusCode Configuration::CManager::GenerateConfigurationFile()
{
    // If the configuration has not been provided to the Configuration Manager
    // generate a configuration object from user input
    if (m_settings.endpoints.empty()) {
        GetConfigurationOptionsFromUser();
    }

    ValidateSettings();

    StatusCode const status = Save();
    if (status != StatusCode::Success) {
        NodeUtils::printo(
            "Failed to save configuration settings to: " + m_filepath.string(),
            NodeUtils::PrintType::Node);
    }

    return status;
}

//-----------------------------------------------------------------------------------------------

std::filesystem::path Configuration::CManager::GetDefaultConfigurationPath() const
{
    std::string filepath = local::FallbackConfigPath; // Set the filepath root to /etc/ by default

    // Attempt to get the $XDG_CONFIG_HOME enviroment variable to use as the configuration directory
    // If $XDG_CONFIG_HOME does not exist try to get the user home directory 
    auto const systemConfigHomePath = std::getenv("XDG_CONFIG_HOME");
    if (systemConfigHomePath) { 
        std::size_t const length = strlen(systemConfigHomePath);
        filepath = std::string(systemConfigHomePath, length);
    } else {
        // Attempt to get the $HOME enviroment variable to use the the configuration directory
        // If $HOME does not exist continue with the fall configuration path
        auto const userHomePath = std::getenv("HOME");
        if (userHomePath) {
            std::size_t const length = strlen(userHomePath);
            filepath = std::string(userHomePath, length) + "/.config";
        } 
    }

    // Concat /brypt/ to the condiguration folder path
    filepath += local::BryptConfigFolder;

    return filepath/DefaultConfigFilename;
}

//-----------------------------------------------------------------------------------------------

void Configuration::CManager::CreateConfigurationFilePath()
{
    auto const filename = (m_filepath.has_filename()) ? m_filepath.filename() : DefaultConfigFilename;
    auto const base = m_filepath.parent_path();

    // Create configuration directories if they do not yet exist on the system
    if (!std::filesystem::exists(base)) {
        // Create directories in the base path that do not exist
        bool const status = std::filesystem::create_directories(base);
        if (!status) {
            NodeUtils::printo(
                "There was an error creating the configuration path: " + base.string(),
                NodeUtils::PrintType::Node);
        }
        // Set the permissions on the brypt conffiguration folder such that only the 
        // user can read, write, and execute
        std::filesystem::permissions(base, std::filesystem::perms::owner_all);
    }

    // Return the base path appended with the configuration filename
    m_filepath =  base/filename;
}

//-----------------------------------------------------------------------------------------------

std::optional<Configuration::TSettings> Configuration::CManager::GetConfiguration() const
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

    std::vector<Configuration::TEndpointOptions> const endpointOptions = 
        local::GetEndpointOptionsFromUser();

    Configuration::TSecurityOptions const securityOptions = 
        local::GetSecurityOptionsFromUser();

    m_settings.details = detailsOptions;
    m_settings.endpoints = endpointOptions;
    m_settings.security = securityOptions;
}

//-----------------------------------------------------------------------------------------------

void Configuration::CManager::InitializeSettings()
{
    std::for_each(m_settings.endpoints.begin(), m_settings.endpoints.end(),
    [](auto& endpoint){
        endpoint.technology = NodeUtils::ParseTechnologyType(endpoint.technology_name);
    });
}

//-----------------------------------------------------------------------------------------------

bool local::IsNewlineOrTab(char c)
{
    switch (c) {
        case '\n':
        case '\t':
            return true;
        default:
            return false;
    }
}

//-----------------------------------------------------------------------------------------------

void local::WriteNodeOptions(
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

void local::WriteEndpointOptions(
    std::vector<Configuration::TEndpointOptions> const& options,
    std::ofstream& out)
{
    out << "\t\"endpoints\": [\n";
    for (auto const& endpoint: options) {
        out << "\t\t{\n";
        out << "\t\t\t\"technology_name\": \"" << NodeUtils::TechnologyTypeToString(endpoint.technology) << "\",\n";
        out << "\t\t\t\"interface\": \"" << endpoint.interface << "\",\n";
        out << "\t\t\t\"binding\": \"" << endpoint.binding << "\",\n";
        out << "\t\t\t\"entry_address\": \"" << endpoint.entry_address << "\"\n";
        out << "\t\t}";
        if (&endpoint != &options.back()) {
            out << ",\n";
        }
    }
    out << "\n\t],\n";
}

//-----------------------------------------------------------------------------------------------

void local::WriteSecurityOptions(
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

std::vector<Configuration::TEndpointOptions> local::GetEndpointOptionsFromUser()
{
    std::vector<Configuration::TEndpointOptions> options;

    bool bAddEndpointOption = false;
    NodeUtils::NodeIdType nodeId = 0;
    do {
        Configuration::TEndpointOptions endpoint(
            ++nodeId,
            defaults::TechnologyName,
            defaults::NetworkInterface,
            defaults::TcpBindingAddress,
            defaults::EntryAddress
        );

        // Get the desired primary technology type for the node
        std::string sTechnology = "";
        std::cout << "Communication Technology: (" << defaults::TechnologyName << ") " << std::flush;
        std::getline(std::cin, sTechnology);
        if (!sTechnology.empty()) {
            endpoint.technology_name = sTechnology;
            endpoint.technology = NodeUtils::ParseTechnologyType(sTechnology);
        }

        // Get the network interface that the node will be bound too
        std::string sInterface = "";
        std::cout << "Network Interface: (" << defaults::NetworkInterface << ") " << std::flush;
        std::getline(std::cin, sInterface);
        if (!sInterface.empty()) {
            endpoint.interface = sInterface;
        }

        // Get the primary and secondary network address components
        // Currently, this may be the IP and port for TCP/IP based nodes
        // or frequency and channel for LoRa.
        std::string sBinding = "";
        std::string bindingOutputMessage = "Binding Address [IP:Port]: (";
        bindingOutputMessage.append(defaults::TcpBindingAddress.data());
        if (endpoint.technology == NodeUtils::TechnologyType::LoRa) {
            bindingOutputMessage = "Binding Frequency: [Frequency:Channel]: (";
            bindingOutputMessage.append(defaults::LoRaBindingAddress.data());
            endpoint.binding = defaults::LoRaBindingAddress;
            endpoint.entry_address = "";
        }
        bindingOutputMessage.append(") ");
        std::cout << bindingOutputMessage << std::flush;
        std::getline(std::cin, sBinding);
        if (!sBinding.empty()) {
            endpoint.binding = sBinding;
        }

        // If the technology type is LoRa, return because the cluster is assumed
        // to be on the same frequency and channel.
        if (endpoint.technology != NodeUtils::TechnologyType::LoRa) {
            // Get the entry address of the network this may be the cluster
            // coordinator. In the future some sort of discovery.
            std::string sEntryAddress = "";
            std::cout << "Entry Address: (" << defaults::EntryAddress << ") " << std::flush;
            std::getline(std::cin, sEntryAddress);
            if (!sEntryAddress.empty()) {
                endpoint.entry_address = sEntryAddress;
            }
        }

        options.emplace_back(endpoint);

        std::string sContinueChoice;
        std::cout << "Enter any key to setup a key (Press enter to continue): " << std::flush;
        std::getline(std::cin, sContinueChoice);
        bAddEndpointOption = !sContinueChoice.empty();
        std::cout << "\n";
    } while(bAddEndpointOption);

    return options;
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
