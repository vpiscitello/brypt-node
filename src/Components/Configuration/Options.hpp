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
    enum Type : std::uint32_t { Invalid, Ephemeral, Persistent};
    struct ConstructedValues { Type type; Node::SharedIdentifier value; };

    Identifier();
    explicit Identifier(std::string_view type, std::string_view value = "");

    void Merge(Identifier& other);
    [[nodiscard]] bool Initialize();

    std::string type;
    std::optional<std::string> value;

    ConstructedValues constructed;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to establish an identifier for the brypt node. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Details
{
    Details();
    explicit Details(std::string_view name, std::string_view description = "", std::string_view location = "");

    void Merge(Details& other);

    std::string name;
    std::string description;
    std::string location;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to create an endpoint. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Endpoint
{
    struct ConstructedValues { 
        Network::Protocol protocol;
        Network::BindingAddress binding;
        std::optional<Network::RemoteAddress> bootstrap;
    };

    Endpoint();
    Endpoint(std::string_view protocol, std::string_view interface, std::string_view binding);

    void Merge(Endpoint& other);
    [[nodiscard]] bool Initialize();

    [[nodiscard]] Network::Protocol GetProtocol() const;
    [[nodiscard]] std::string const& GetProtocolString() const;
    [[nodiscard]] std::string const& GetInterface() const;
    [[nodiscard]] Network::BindingAddress const& GetBinding() const;
    [[nodiscard]] std::optional<Network::RemoteAddress> const& GetBootstrap() const;

    std::string protocol;
    std::string interface;
    std::string binding;
    std::optional<std::string> bootstrap;

    ConstructedValues constructed;
};

//----------------------------------------------------------------------------------------------------------------------
// Description: The set of options used to configure the security strategy. 
//----------------------------------------------------------------------------------------------------------------------
struct Configuration::Options::Security
{
    struct ConstructedValues { ::Security::Strategy strategy; };

    Security();
    Security(std::string_view strategy, std::string_view token);

    void Merge(Security& other);
    [[nodiscard]] bool Initialize();

    std::string strategy;
    std::string token;

    ConstructedValues constructed;
};

//----------------------------------------------------------------------------------------------------------------------
