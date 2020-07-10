//------------------------------------------------------------------------------------------------
// File: Configuration.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Utilities/NetworkUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
#include "../Utilities/Version.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Configuration {
//------------------------------------------------------------------------------------------------

struct TSettings;
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

struct Configuration::TDetailsOptions
{
    TDetailsOptions()
        : version(Brypt::Version)
        , name()
        , description()
        , location()
    {
    }

    TDetailsOptions(
        std::string_view name,
        std::string_view description = "",
        std::string_view location = "")
        : version(Brypt::Version)
        , name(name)
        , description(description)
        , location(location)
    {
    }

    std::string version;
    std::string name;
    std::string description;
    std::string location;
};

//------------------------------------------------------------------------------------------------

struct Configuration::TEndpointOptions
{
    TEndpointOptions()
        : technology(Endpoints::TechnologyType::Invalid)
        , technology_name()
        , interface()
        , binding()
    {
    }

    TEndpointOptions(
        std::string_view technology_name,
        std::string_view interface,
        std::string_view binding)
        : technology(Endpoints::TechnologyType::Invalid)
        , technology_name(technology_name)
        , interface(interface)
        , binding(binding)
    {
        technology = Endpoints::ParseTechnologyType(technology_name.data());
    }

    TEndpointOptions(
        Endpoints::TechnologyType technology,
        std::string_view interface,
        std::string_view binding)
        : technology(technology)
        , technology_name()
        , interface(interface)
        , binding(binding)
    {
        technology_name = Endpoints::TechnologyTypeToString(technology);
    }

    Endpoints::TechnologyType GetTechnology() const { return technology; }
    std::string GetTechnologyName() const { return technology_name; }
    std::string GetInterface() const { return interface; }
    std::string GetBinding() const { return binding; }

    NetworkUtils::AddressComponentPair GetBindingComponents() const
    {
        auto components = NetworkUtils::SplitAddressString(binding);
        switch (technology) {
            case Endpoints::TechnologyType::Direct:
            case Endpoints::TechnologyType::StreamBridge:
            case Endpoints::TechnologyType::TCP: {
                if (auto const found = components.first.find(NetworkUtils::Wildcard); found != std::string::npos) {
                    components.first = NetworkUtils::GetInterfaceAddress(interface);
                }
            } break;
            default: break; // Do not do any additional processing on types that don't require it
        }

        return components;
    }

    Endpoints::TechnologyType technology;
    std::string technology_name;
    std::string interface;
    std::string binding;
};

//------------------------------------------------------------------------------------------------

struct Configuration::TSecurityOptions
{
    TSecurityOptions()
        : standard()
        , token()
        , central_authority()
    {
    }

    TSecurityOptions(
        std::string_view standard,
        std::string_view token,
        std::string_view central_authority)
        : standard(standard)
        , token(token)
        , central_authority(central_authority)
    {
    }

    std::string standard;
    std::string token;
    std::string central_authority;
};

//------------------------------------------------------------------------------------------------

struct Configuration::TSettings
{
    TSettings()
        : details()
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

    TDetailsOptions details;
    EndpointConfigurations endpoints;
    TSecurityOptions security;
};

//------------------------------------------------------------------------------------------------
