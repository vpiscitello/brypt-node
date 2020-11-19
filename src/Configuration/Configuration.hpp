//------------------------------------------------------------------------------------------------
// File: Configuration.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../BryptIdentifier/BryptIdentifier.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Components/Security/SecurityDefinitions.hpp"
#include "../Components/Security/SecurityUtils.hpp"
#include "../Utilities/NetworkUtils.hpp"
#include "../Utilities/Version.hpp"
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

struct TSettings;
struct TIdentifierOptions;
struct TDetailsOptions;
struct TEndpointOptions;
struct TSecurityOptions;

using EndpointConfigurations = std::vector<TEndpointOptions>;

std::filesystem::path const DefaultBryptFolder = "/brypt/";
std::filesystem::path const DefaultConfigurationFilename = "config.json";
std::filesystem::path const DefaultKnownPeersFilename = "peers.json";

std::filesystem::path GetDefaultBryptFolder();
std::filesystem::path GetDefaultConfigurationFilepath();
std::filesystem::path GetDefaultPeersFilepath();

//------------------------------------------------------------------------------------------------
} // Configuration namespace
//------------------------------------------------------------------------------------------------

struct Configuration::TIdentifierOptions
{
    TIdentifierOptions()
        : value()
        , type()
        , container()
    {
    }

    TIdentifierOptions(std::string_view type)
        : value()
        , type(type)
        , container()
    {
    }

    TIdentifierOptions(
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

struct Configuration::TDetailsOptions
{
    TDetailsOptions()
        : name()
        , description()
        , location()
    {
    }

    TDetailsOptions(
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

struct Configuration::TEndpointOptions
{
    TEndpointOptions()
        : type(Endpoints::TechnologyType::Invalid)
        , technology()
        , interface()
        , binding()
        , bootstrap()
    {
    }

    TEndpointOptions(
        std::string_view technology,
        std::string_view interface,
        std::string_view binding)
        : type(Endpoints::TechnologyType::Invalid)
        , technology(technology)
        , interface(interface)
        , binding(binding)
        , bootstrap()
    {
        type = Endpoints::ParseTechnologyType({technology.data(), technology.size()});
    }

    TEndpointOptions(
        Endpoints::TechnologyType type,
        std::string_view interface,
        std::string_view binding)
        : type(type)
        , technology()
        , interface(interface)
        , binding(binding)
        , bootstrap()
    {
        technology = Endpoints::TechnologyTypeToString(type);
    }

    Endpoints::TechnologyType GetTechnology() const { return type; }
    std::string const& GetTechnologyName() const { return technology; }
    std::string const& GetInterface() const { return interface; }
    std::string const& GetBinding() const { return binding; }
    std::optional<std::string> const& GetBootstrap() const { return bootstrap; }

    NetworkUtils::AddressComponentPair GetBindingComponents() const
    {
        auto components = NetworkUtils::SplitAddressString(binding);
        switch (type) {
            case Endpoints::TechnologyType::TCP: {
                if (auto const found = components.first.find(NetworkUtils::Wildcard); found != std::string::npos) {
                    components.first = NetworkUtils::GetInterfaceAddress(interface);
                }
            } break;
            default: break; // Do not do any additional processing on types that don't require it
        }

        return components;
    }

    Endpoints::TechnologyType type;
    std::string technology;
    std::string interface;
    std::string binding;
    std::optional<std::string> bootstrap;
};

//------------------------------------------------------------------------------------------------

struct Configuration::TSecurityOptions
{
    TSecurityOptions()
        : type(Security::Strategy::Invalid)
        , strategy()
        , token()
        , authority()
    {
    }

    TSecurityOptions(
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

struct Configuration::TSettings
{
    TSettings()
        : version(Brypt::Version)
        , details()
        , endpoints()
        , security()
    {
    }

    TSettings(
        TDetailsOptions const& detailsOptions,
        EndpointConfigurations const& endpointsConfigurations,
        TSecurityOptions const& securityOptions)
        : details(detailsOptions)
        , endpoints(endpointsConfigurations)
        , security(securityOptions)
    {
    }

    TSettings(TSettings const& other)
        : details(other.details)
        , endpoints(other.endpoints)
        , security(other.security)
    {
    }

    TSettings& operator=(TSettings const& other)
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

    TIdentifierOptions const& GetIdentifierOptions()
    {
        return identifier;
    }

    TDetailsOptions const& GetDetailsOptions()
    {
        return details;
    }

    EndpointConfigurations const& GetEndpointConfigurations()
    {
        return endpoints;
    }

    TSecurityOptions const& GetSecurityOptions()
    {
        return security;
    }

    std::string version;
    TIdentifierOptions identifier;
    TDetailsOptions details;
    EndpointConfigurations endpoints;
    TSecurityOptions security;
};

//------------------------------------------------------------------------------------------------
