//----------------------------------------------------------------------------------------------------------------------
// File: Parser.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Parser.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/StartupOptions.hpp"
#include "Components/Network/Protocol.hpp"
#include "Utilities/FileUtils.hpp"
#include "Utilities/Logger.hpp"
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

[[nodiscard]] bool InitializeIdentifier(Configuration::Options::Identifier& options);
[[nodiscard]] bool InitializeEndpoints(
    Configuration::Options::Endpoints& endpoints, std::shared_ptr<spdlog::logger> const& logger);
void InitializeSecurityOptions(Configuration::Options::Security& options);

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

struct local::OptionsStore
{
    std::string version;
    Configuration::Options::Identifier identifier;
    Configuration::Options::Details details;
    Configuration::Options::Endpoints endpoints;
    Configuration::Options::Security security;
};

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
{
    assert(m_logger);

    // If we are allowed to deduce the filepath, update the configured path using the defaults when applicable.
    if (m_runtime.useFilepathDeduction) {
        // If the filepath does not have a filename, attach the default config.json
        if (!m_filepath.has_filename()) { m_filepath = m_filepath / DefaultConfigurationFilename; }

        // If the filepath does not have a parent path, get and attach the default brypt folder
        if (!m_filepath.has_parent_path()) {  m_filepath = GetDefaultBryptFolder() / m_filepath; }
    }

    // If we are allowed to generate the missing file based on user input, check and create the folders in the 
    // filesystem if they do not exist. If we fail to create the path, log out an error message.
    if (m_runtime.useInteractiveConsole && !FileUtils::CreateFolderIfNoneExist(m_filepath)) {
        m_logger->error("Failed to create the filepath at: {}!", m_filepath.string());
    }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::FetchOptions()
{
    StatusCode status;
    if (std::filesystem::exists(m_filepath)) {
        // If we have been provided a path to an existing file, deserialize the options. 
        m_logger->debug("Reading configuration file at: {}.", m_filepath.string());
        status = Deserialize();
    } else if (m_runtime.useInteractiveConsole) {
        // If we have not been provided a path to a configutation file, deserialize the options. 
        m_logger->warn(
            "A configuration file could not be found. Launching configuration generator for: {}.", m_filepath.string());
        status = LaunchGenerator();
    } else {
        std::ostringstream oss;
        oss << "Unable to locate: {}; The configuration generator could not be launched ";
        oss << "with \"--non-interactive\" enabled.";
        m_logger->error(oss.str(), m_filepath.string());
        status = StatusCode::FileError;
    }

    // Return early to capture the correct status code.
    if (status != StatusCode::Success) {
        return status;
    } 

    // If the options could be initialized, indicate a successful parsing. Otherwise, an input error occured. 
    return (InitializeOptions() ? StatusCode::Success : StatusCode::InputError);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::Serialize()
{
    if (!m_validated) { return StatusCode::InputError; }
    if (m_filepath.empty()) { return StatusCode::FileError; }

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

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

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
        m_logger->error("Failed to save configuration options to: {}!", m_filepath.string());
        return status;
    }

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

RuntimeContext Configuration::Parser::GetRuntimeContext() const
{
    return m_runtime.context;
}

//----------------------------------------------------------------------------------------------------------------------

spdlog::level::level_enum Configuration::Parser::GetVerbosityLevel() const
{
    return m_runtime.verbosity;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::UseInteractiveConsole() const
{
    return m_runtime.useInteractiveConsole;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::UseBootstraps() const
{
    return m_runtime.useBootstraps;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::UseFilepathDeduction() const
{
    return m_runtime.useFilepathDeduction;
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Configuration::Parser::GetNodeIdentifier() const
{
    assert(m_validated);
    assert(m_identifier.spConstructedValue && m_identifier.spConstructedValue->IsValid());
    return m_identifier.spConstructedValue;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeName() const
{
    assert(m_validated);
    return m_details.name;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeDescription() const
{
    assert(m_validated);
    return m_details.description;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetNodeLocation() const
{
    assert(m_validated);
    return m_details.location;
}

//----------------------------------------------------------------------------------------------------------------------
    
Configuration::Options::Endpoints const& Configuration::Parser::GetEndpointOptions() const
{
    assert(m_validated);
    assert(m_endpoints.size() != 0);
    return m_endpoints;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Strategy Configuration::Parser::GetSecurityStrategy() const
{
    assert(m_validated);
    assert(m_security.type != Security::Strategy::Invalid);
    return m_security.type;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Parser::GetCentralAuthority() const
{
    assert(m_validated);
    return m_security.authority;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::IsValidated() const
{
    return m_validated;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::ValidateOptions()
{
    m_validated = false;

    if (m_identifier.type.empty()) { return StatusCode::DecodeError; }
    if (!allowable::IfAllowableGetValue(allowable::IdentifierTypes, m_identifier.type)) {
        return StatusCode::DecodeError;
    }

    if (m_endpoints.empty()) { return StatusCode::DecodeError; }
    for (auto const& endpoint: m_endpoints) {
        if (!allowable::IfAllowableGetValue(allowable::EndpointTypes, endpoint.protocol)) {
            return StatusCode::DecodeError;
        }
    }

    if (!allowable::IfAllowableGetValue(allowable::StrategyTypes, m_security.strategy)) {
        return StatusCode::DecodeError;
    }

    m_validated = true;

    return StatusCode::Success;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::StatusCode Configuration::Parser::Deserialize()
{
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
        s::security = li::json_object(s::strategy, s::token, s::authority))
        .decode(json, store);
    if (error.code) { return StatusCode::DecodeError; }

    m_version = std::move(store.version);
    m_identifier = std::move(store.identifier);
    m_details = std::move(store.details);
    m_endpoints = std::move(store.endpoints);
    m_security = std::move(store.security);

    // Valdate the decoded settings. If the settings decoded are not valid return noopt.
    return ValidateOptions();
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Parser::GetOptionsFromUser()
{
    std::cout << "Generating Brypt Node Configuration Settings." << std::endl; 
    std::cout << "Please Enter your Desired Network Options.\n" << std::endl;

    // Clear the cin stream 
    std::cin.clear(); std::cin.sync();

    m_identifier = local::GetIdentifierFromUser();
    m_details = local::GetDetailFromUser();
    m_endpoints = local::GetEndpointFromUser();
    m_security = local::GetSecurityFromUser();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Parser::InitializeOptions()
{
    if (!local::InitializeIdentifier(m_identifier)) { return false; }
    if (!local::InitializeEndpoints(m_endpoints, m_logger)) { return false; }

    local::InitializeSecurityOptions(m_security);

    // Update the configuration file as the initialization of options may create new values 
    // for certain options. Currently, this only caused by the generation of Brypt Identifiers.
    auto const status = Serialize();
    if (status != StatusCode::Success) {
        m_logger->error("Failed to update configuration file at: {}!", m_filepath.string());
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
    out << "\t\t\"token\": \"" << options.token << "\",\n";
    out << "\t\t\"authority\": \"" << options.authority << "\"\n";
    out << "\t}\n";
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier local::GetIdentifierFromUser()
{
    Configuration::Options::Identifier options(defaults::IdentifierType);

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
            return local::GetIdentifierFromUser();
        }
    }
    std::cout << std::endl;

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

Configuration::Options::Endpoints local::GetEndpointFromUser()
{
    Configuration::Options::Endpoints endpoints;

    bool bAddEndpointOption = false;
    do {
        Configuration::Options::Endpoint options(
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

Configuration::Options::Security local::GetSecurityFromUser()
{
    Configuration::Options::Security options(
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

bool local::InitializeIdentifier(Configuration::Options::Identifier& options)
{
    // If the identifier type is Ephemeral, then a new identifier should be generated. 
    // If the identifier type is Persistent, the we shall attempt to use provided value.
    // Would should never hit this function before the the options have been validated.
    if (options.type == "Ephemeral") {
        // Generate a new Brypt Identifier and store the network representation as the value.
        options.spConstructedValue = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
        assert(options.spConstructedValue);
        options.value = *options.spConstructedValue;
    } else if (options.type == "Persistent") {
        // If an identifier value has been parsed, attempt to use the provided value as
        // the identifier. We need to check the validity of the identifier to ensure the
        // value was properly formatted. 
        // Otherwise, a new Brypt Identifier must be be generated. 
        if (options.value) {
            options.spConstructedValue = std::make_shared<Node::Identifier const>(*options.value);
            assert(options.spConstructedValue);
            if (!options.spConstructedValue->IsValid()) {
                options.value.reset();
                return false;
            }
        } else {
            options.spConstructedValue = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
            assert(options.spConstructedValue);
            options.value = *options.spConstructedValue;  
        }
    } else {
        assert(false); // How did these identifier options pass validation?
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool local::InitializeEndpoints(
    Configuration::Options::Endpoints& endpoints, std::shared_ptr<spdlog::logger> const& logger)
{
    bool success = true;
    std::for_each(endpoints.begin(), endpoints.end(),
        [&success, &logger] (auto& options)
        {
            if (!options.Initialize()) {
                logger->warn("Unable to initialize the endpoint configuration for {}", options.GetProtocolName());
                success = false;
            }
        });
    return success;
}

//----------------------------------------------------------------------------------------------------------------------

void local::InitializeSecurityOptions(Configuration::Options::Security& options)
{
    options.type = Security::ConvertToStrategy(options.strategy);
}

//----------------------------------------------------------------------------------------------------------------------
