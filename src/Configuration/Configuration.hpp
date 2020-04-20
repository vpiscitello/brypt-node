//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NetworkUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
#include "../Utilities/Version.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <cstring>
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

//------------------------------------------------------------------------------------------------
} // Configuration namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

struct Configuration::TDetailsOptions
{
    TDetailsOptions()
        : version(Brypt::Version)
        , name()
        , description()
        , location()
        , operation(NodeUtils::DeviceOperation::None)
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
        , operation(NodeUtils::DeviceOperation::None)
    {
    }

    std::string version;
    std::string name;
    std::string description;
    std::string location;
    NodeUtils::DeviceOperation operation;
};

//------------------------------------------------------------------------------------------------

struct Configuration::TEndpointOptions
{
    TEndpointOptions()
        : id(0)
        , technology(NodeUtils::TechnologyType::None)
        , technology_name()
        , operation(NodeUtils::EndpointOperation::None)
        , interface()
        , binding()
        , entry()
    {
    }

    TEndpointOptions(
        NodeUtils::NodeIdType id,
        std::string_view technology_name,
        std::string_view interface,
        std::string_view binding,
        std::string_view entry = std::string())
        : id(id)
        , technology(NodeUtils::TechnologyType::None)
        , technology_name(technology_name)
        , operation(NodeUtils::EndpointOperation::None)
        , interface(interface)
        , binding(binding)
        , entry(entry)
    {
        technology = NodeUtils::ParseTechnologyType(technology_name.data());
    }

    TEndpointOptions(
        NodeUtils::NodeIdType id,
        NodeUtils::TechnologyType technology,
        std::string_view interface,
        std::string_view binding,
        std::string_view entry = std::string())
        : id(id)
        , technology(technology)
        , technology_name()
        , operation(NodeUtils::EndpointOperation::None)
        , interface(interface)
        , binding(binding)
        , entry(entry)
    {
        technology_name = NodeUtils::TechnologyTypeToString(technology);
    }

    std::string GetBinding() const { return binding; }
    std::string GetEntry() const { return entry; }

    NetworkUtils::AddressComponentPair GetBindingComponents() const
    {
        auto components = NetworkUtils::SplitAddressString(binding);
        switch (technology) {
            case NodeUtils::TechnologyType::Direct:
            case NodeUtils::TechnologyType::StreamBridge:
            case NodeUtils::TechnologyType::TCP: {
                if (auto const found = components.first.find(NetworkUtils::Wildcard); found != std::string::npos) {
                    components.first = NetworkUtils::GetInterfaceAddress(interface);
                }
            } break;
            default: break; // Do not do any additional processing on types that don't require it
        }

        return components;
    }

    NetworkUtils::AddressComponentPair GetEntryComponents() const
    {
        return NetworkUtils::SplitAddressString(entry);
    }

    NodeUtils::NodeIdType id;
    NodeUtils::TechnologyType technology;
    std::string technology_name;
    NodeUtils::EndpointOperation operation;
    std::string interface;
    std::string binding;
    std::string entry;
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
        std::vector<TEndpointOptions> const& endpointsOptions,
        TSecurityOptions const& securityOptions)
        : details(detailsOptions)
        , endpoints(endpointsOptions)
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
    std::vector<TEndpointOptions> endpoints;
    TSecurityOptions security;
};

//------------------------------------------------------------------------------------------------
