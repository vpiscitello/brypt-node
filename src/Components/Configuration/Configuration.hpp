//----------------------------------------------------------------------------------------------------------------------
// File: Configuration.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Network/Address.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Utilities/Version.hpp"
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

struct Settings;
struct IdentifierOptions;
struct DetailsOptions;
struct EndpointOptions;
struct SecurityOptions;

using EndpointsSet = std::vector<EndpointOptions>;

std::filesystem::path const DefaultBryptFolder = "/brypt/";
std::filesystem::path const DefaultConfigurationFilename = "config.json";
std::filesystem::path const DefaultKnownPeersFilename = "peers.json";

std::filesystem::path GetDefaultBryptFolder();
std::filesystem::path GetDefaultConfigurationFilepath();
std::filesystem::path GetDefaultPeersFilepath();

//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------

struct Configuration::IdentifierOptions
{
    IdentifierOptions();
    explicit IdentifierOptions(std::string_view type);
    IdentifierOptions(std::string_view value, std::string_view type);

    std::optional<std::string> value;
    std::string type;

    Node::SharedIdentifier container;
};

//----------------------------------------------------------------------------------------------------------------------

struct Configuration::DetailsOptions
{
    DetailsOptions();
    explicit DetailsOptions(
        std::string_view name,
        std::string_view description = "",
        std::string_view location = "");

    std::string name;
    std::string description;
    std::string location;
};

//----------------------------------------------------------------------------------------------------------------------

struct Configuration::EndpointOptions
{
    EndpointOptions();
    EndpointOptions(
        std::string_view protocol,
        std::string_view interface,
        std::string_view binding);
    EndpointOptions(
        Network::Protocol type,
        std::string_view interface,
        std::string_view binding);

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

struct Configuration::SecurityOptions
{
    SecurityOptions();
    SecurityOptions(
        std::string_view strategy,
        std::string_view token,
        std::string_view authority);

    Security::Strategy type;
    std::string strategy;
    std::string token;
    std::string authority;
};

//----------------------------------------------------------------------------------------------------------------------

struct Configuration::Settings
{
    Settings();
    Settings(
        DetailsOptions const& details,
        EndpointsSet const& endpoints,
        SecurityOptions const& security);

    Settings(Settings const& other) = default;
    Settings& operator=(Settings const& other) = default;

    [[nodiscard]] std::string const& GetVersion() const;
    [[nodiscard]] IdentifierOptions const& GetIdentifierOptions() const;
    [[nodiscard]] DetailsOptions const& GetDetailsOptions() const;
    [[nodiscard]] EndpointsSet const& GetEndpointOptions() const;
    [[nodiscard]] SecurityOptions const& GetSecurityOptions()  const;

    std::string version;
    IdentifierOptions identifier;
    DetailsOptions details;
    EndpointsSet endpoints;
    SecurityOptions security;
};

//----------------------------------------------------------------------------------------------------------------------
