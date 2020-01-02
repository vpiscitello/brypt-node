//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "../Utilities/NodeUtils.hpp"
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
struct TConnectionOptions;
struct TSecurityOptions;

//------------------------------------------------------------------------------------------------
} // Configuration namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

struct Configuration::TDetailsOptions
{
    TDetailsOptions()
        : version(NodeUtils::NODE_VERSION)
        , name()
        , description()
        , location()
    {
    }

    TDetailsOptions(
        std::string_view name,
        std::string_view description,
        std::string_view location)
        : version(NodeUtils::NODE_VERSION)
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

struct Configuration::TConnectionOptions
{
    TConnectionOptions()
        : technology(NodeUtils::TechnologyType::NONE)
        , technology_name()
        , operation(NodeUtils::DeviceOperation::NONE)
        , interface()
        , binding()
        , entry_address()
    {
    }

    TConnectionOptions(
        std::string_view technology_name,
        std::string_view interface,
        std::string_view binding,
        std::string_view entryAddress)
        : technology(NodeUtils::TechnologyType::NONE)
        , technology_name(technology_name)
        , operation(NodeUtils::DeviceOperation::NONE)
        , interface(interface)
        , binding(binding)
        , entry_address(entryAddress)
    {
        technology = NodeUtils::ParseTechnologyType(technology_name.data());
    }

    TConnectionOptions(
        NodeUtils::TechnologyType technology,
        std::string_view interface,
        std::string_view binding,
        std::string_view entryAddress)
        : technology(technology)
        , technology_name()
        , operation(NodeUtils::DeviceOperation::NONE)
        , interface(interface)
        , binding(binding)
        , entry_address(entryAddress)
    {
        technology_name = NodeUtils::TechnologyTypeToString(technology);
    }

    NodeUtils::AddressComponentPair GetBindingComponents() const
    {
        return NodeUtils::SplitAddressString(binding);
    }

    NodeUtils::AddressComponentPair GetEntryComponents() const
    {
        return NodeUtils::SplitAddressString(entry_address);
    }

    NodeUtils::TechnologyType technology;
    std::string technology_name;
    NodeUtils::DeviceOperation operation;
    std::string interface;
    std::string binding;
    std::string entry_address;
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
        , connections()
        , security()
    {
    }

    TSettings(
        TDetailsOptions const& detailsOptions,
        std::vector<TConnectionOptions> const& connectionsOptions,
        TSecurityOptions const& securityOptions)
        : details(detailsOptions)
        , connections(connectionsOptions)
        , security(securityOptions)
    {
    }

    TSettings(TSettings const& other)
        : details(other.details)
        , connections(other.connections)
        , security(other.security)
    {
    }

    TSettings& operator=(TSettings const& other)
    {
        details = other.details;
        connections = other.connections;
        security = other.security;
        return *this;
    }

    TDetailsOptions details;
    std::vector<TConnectionOptions> connections;
    TSecurityOptions security;
};

//------------------------------------------------------------------------------------------------
