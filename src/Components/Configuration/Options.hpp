//----------------------------------------------------------------------------------------------------------------------
// File: Options.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptNode/RuntimeContext.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Utilities/Version.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/common.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Configuration {
//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path const DefaultBryptFolder = "/brypt/";
std::filesystem::path const DefaultConfigurationFilename = "config.json";
std::filesystem::path const DefaultBootstrapFilename = "bootstrap.json";

std::filesystem::path GetDefaultBryptFolder();
std::filesystem::path GetDefaultConfigurationFilepath();
std::filesystem::path GetDefaultBootstrapFilepath();

//----------------------------------------------------------------------------------------------------------------------
namespace Options {
//----------------------------------------------------------------------------------------------------------------------

struct Runtime;
struct Identifier;
struct Details;
struct Endpoint;
struct Security;

using Endpoints = std::vector<Endpoint>;

//----------------------------------------------------------------------------------------------------------------------
} // Options namespace
//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options defined at runtime (e.g. cli flags or or shared library methods).
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Runtime
{
    RuntimeContext context;
    spdlog::level::level_enum verbosity;
    bool useInteractiveConsole;
    bool useBootstraps;
    bool useFilepathDeduction;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options defined at runtime (e.g. cli flags or or shared library methods).
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Identifier
{
    Identifier();
    explicit Identifier(std::string_view type);
    Identifier(std::string_view value, std::string_view type);

    std::optional<std::string> value;
    std::string type;
    Node::SharedIdentifier spConstructedValue;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to establish an identifier for the brypt node. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Details
{
    Details();
    explicit Details(std::string_view name, std::string_view description = "", std::string_view location = "");

    std::string name;
    std::string description;
    std::string location;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to create an endpoint. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Endpoint
{
    Endpoint();
    Endpoint(std::string_view protocol, std::string_view interface, std::string_view binding);
    Endpoint(Network::Protocol type, std::string_view interface, std::string_view binding);

    [[nodiscard]] bool Initialize();

    [[nodiscard]] Network::Protocol GetProtocol() const;
    [[nodiscard]] std::string const& GetProtocolName() const;
    [[nodiscard]] std::string const& GetInterface() const;
    [[nodiscard]] Network::BindingAddress const& GetBinding() const;
    [[nodiscard]] std::optional<Network::RemoteAddress> const& GetBootstrap() const;

    Network::Protocol type;
    std::string protocol;
    std::string interface;

    std::string binding;
    Network::BindingAddress bindingAddress;

    std::optional<std::string> bootstrap;
    std::optional<Network::RemoteAddress> optBootstrapAddress;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure the security strategy. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Security
{
    Security();
    Security(std::string_view strategy, std::string_view token, std::string_view authority);

    ::Security::Strategy type;
    std::string strategy;
    std::string token;
    std::string authority;
};

//----------------------------------------------------------------------------------------------------------------------
