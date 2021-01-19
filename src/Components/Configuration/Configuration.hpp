//------------------------------------------------------------------------------------------------
// File: Configuration.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SecurityUtils.hpp"
#include "Utilities/NetworkUtils.hpp"
#include "Utilities/Version.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Configuration {
//------------------------------------------------------------------------------------------------

struct Settings;
struct IdentifierOptions;
struct DetailsOptions;
struct EndpointOptions;
struct SecurityOptions;

using EndpointConfigurations = std::vector<EndpointOptions>;

std::filesystem::path const DefaultBryptFolder = "/brypt/";
std::filesystem::path const DefaultConfigurationFilename = "config.json";
std::filesystem::path const DefaultKnownPeersFilename = "peers.json";

std::filesystem::path GetDefaultBryptFolder();
std::filesystem::path GetDefaultConfigurationFilepath();
std::filesystem::path GetDefaultPeersFilepath();

//------------------------------------------------------------------------------------------------
} // Configuration namespace
//------------------------------------------------------------------------------------------------

struct Configuration::IdentifierOptions
{
    IdentifierOptions()
        : value()
        , type()
        , container()
    {
    }

    IdentifierOptions(std::string_view type)
        : value()
        , type(type)
        , container()
    {
    }

    IdentifierOptions(
        std::string_view value,
        std::string_view type)
        : value(value)
        , type(type)
        , container()
    {
    }

    std::optional<std::string> value;
    std::string type;

    BryptIdentifier::SharedContainer container;
};

//------------------------------------------------------------------------------------------------

struct Configuration::DetailsOptions
{
    DetailsOptions()
        : name()
        , description()
        , location()
    {
    }

    DetailsOptions(
        std::string_view name,
        std::string_view description = "",
        std::string_view location = "")
        : name(name)
        , description(description)
        , location(location)
    {
    }

    std::string name;
    std::string description;
    std::string location;
};

//------------------------------------------------------------------------------------------------

struct Configuration::EndpointOptions
{
    EndpointOptions()
        : type(Network::Protocol::Invalid)
        , protocol()
        , interface()
        , binding()
        , bootstrap()
    {
    }

    EndpointOptions(
        std::string_view protocol,
        std::string_view interface,
        std::string_view binding)
        : type(Network::Protocol::Invalid)
        , protocol(protocol)
        , interface(interface)
        , binding(binding)
        , bootstrap()
    {
        type = Network::ParseProtocol({protocol.data(), protocol.size()});
    }

    EndpointOptions(
        Network::Protocol type,
        std::string_view interface,
        std::string_view binding)
        : type(type)
        , protocol()
        , interface(interface)
        , binding(binding)
        , bootstrap()
    {
        protocol = Network::ProtocolToString(type);
    }

    Network::Protocol GetProtocol() const { return type; }
    std::string const& GetProtocolName() const { return protocol; }
    std::string const& GetInterface() const { return interface; }
    std::string const& GetBinding() const { return binding; }
    std::optional<std::string> const& GetBootstrap() const { return bootstrap; }

    NetworkUtils::AddressComponentPair GetBindingComponents() const
    {
        auto components = NetworkUtils::SplitAddressString(binding);
        switch (type) {
            case Network::Protocol::TCP: {
                if (auto const found = components.first.find(NetworkUtils::Wildcard); found != std::string::npos) {
                    components.first = NetworkUtils::GetInterfaceAddress(interface);
                }
            } break;
            default: break; // Do not do any additional processing on types that don't require it
        }

        return components;
    }

    Network::Protocol type;
    std::string protocol;
    std::string interface;
    std::string binding;
    std::optional<std::string> bootstrap;
};

//------------------------------------------------------------------------------------------------

struct Configuration::SecurityOptions
{
    SecurityOptions()
        : type(Security::Strategy::Invalid)
        , strategy()
        , token()
        , authority()
    {
    }

    SecurityOptions(
        std::string_view strategy,
        std::string_view token,
        std::string_view authority)
        : type(Security::Strategy::Invalid)
        , strategy(strategy)
        , token(token)
        , authority(authority)
    {
        type = Security::ConvertToStrategy({strategy.data(), strategy.size()});
    }

    Security::Strategy type;
    std::string strategy;
    std::string token;
    std::string authority;
};

//------------------------------------------------------------------------------------------------

struct Configuration::Settings
{
    Settings()
        : version(Brypt::Version)
        , details()
        , endpoints()
        , security()
    {
    }

    Settings(
        DetailsOptions const& detailsOptions,
        EndpointConfigurations const& endpointsConfigurations,
        SecurityOptions const& securityOptions)
        : details(detailsOptions)
        , endpoints(endpointsConfigurations)
        , security(securityOptions)
    {
    }

    Settings(Settings const& other)
        : details(other.details)
        , endpoints(other.endpoints)
        , security(other.security)
    {
    }

    Settings& operator=(Settings const& other)
    {
        details = other.details;
        endpoints = other.endpoints;
        security = other.security;
        return *this;
    }

    std::string const& GetVersion()
    {
        return version;
    }

    IdentifierOptions const& GetIdentifierOptions()
    {
        return identifier;
    }

    DetailsOptions const& GetDetailsOptions()
    {
        return details;
    }

    EndpointConfigurations const& GetEndpointConfigurations()
    {
        return endpoints;
    }

    SecurityOptions const& GetSecurityOptions()
    {
        return security;
    }

    std::string version;
    IdentifierOptions identifier;
    DetailsOptions details;
    EndpointConfigurations endpoints;
    SecurityOptions security;
};

//------------------------------------------------------------------------------------------------
