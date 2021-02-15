//------------------------------------------------------------------------------------------------
// File: Configuration.cpp 
// Description:
//-----------------------------------------------------------------------------------------------
#include "Configuration.hpp"
//-----------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace defaults {
//------------------------------------------------------------------------------------------------

std::filesystem::path const FallbackConfigurationFolder = "/etc/";

//------------------------------------------------------------------------------------------------
} // default namespace
} // namespace
//------------------------------------------------------------------------------------------------

Configuration::IdentifierOptions::IdentifierOptions()
    : value()
    , type()
    , container()
{
}

//------------------------------------------------------------------------------------------------

Configuration::IdentifierOptions::IdentifierOptions(std::string_view type)
    : value()
    , type(type)
    , container()
{
}

//------------------------------------------------------------------------------------------------

Configuration::IdentifierOptions::IdentifierOptions(std::string_view value, std::string_view type)
    : value(value)
    , type(type)
    , container()
{
}

//------------------------------------------------------------------------------------------------

Configuration::DetailsOptions::DetailsOptions()
    : name()
    , description()
    , location()
{
}

//------------------------------------------------------------------------------------------------

Configuration::DetailsOptions::DetailsOptions(
    std::string_view name,
    std::string_view description,
    std::string_view location)
    : name(name)
    , description(description)
    , location(location)
{
}

//------------------------------------------------------------------------------------------------

Configuration::EndpointOptions::EndpointOptions()
    : type(Network::Protocol::Invalid)
    , protocol()
    , interface()
    , binding()
    , bootstrap()
{
}

//------------------------------------------------------------------------------------------------

Configuration::EndpointOptions::EndpointOptions(
    std::string_view protocol,
    std::string_view interface,
    std::string_view binding)
    : type(Network::ParseProtocol({protocol.data(), protocol.size()}))
    , protocol(protocol)
    , interface(interface)
    , binding(binding)
    , bootstrap()
{
}

//------------------------------------------------------------------------------------------------

Configuration::EndpointOptions::EndpointOptions(
    Network::Protocol type,
    std::string_view interface,
    std::string_view binding)
    : type(type)
    , protocol(Network::ProtocolToString(type))
    , interface(interface)
    , binding(binding)
    , bootstrap()
{
}

//------------------------------------------------------------------------------------------------

bool Configuration::EndpointOptions::Initialize()
{
    if (type == Network::Protocol::Invalid && !protocol.empty()) {
        type = Network::ParseProtocol(protocol);
    }

    if (protocol.empty()) { Network::ProtocolToString(type); }

    if (bindingAddress.IsValid()) { return true; }
    if (type == Network::Protocol::Invalid || binding.empty() || interface.empty()) {
        return false; 
    }
    bindingAddress = { type, binding, interface };
    if (!bindingAddress.IsValid()) { return false; }

    if (optBootstrapAddress && optBootstrapAddress->IsValid()) { return true; }
    if (bootstrap && !bootstrap->empty()) {
        optBootstrapAddress = { type, *bootstrap, true };
        if (!optBootstrapAddress->IsValid()) { return false; }
    }

    return true;
}

//------------------------------------------------------------------------------------------------

Network::Protocol Configuration::EndpointOptions::GetProtocol() const
{
    return type;
}

//------------------------------------------------------------------------------------------------

std::string const& Configuration::EndpointOptions::GetProtocolName() const
{
    return protocol;
}

//------------------------------------------------------------------------------------------------

std::string const& Configuration::EndpointOptions::GetInterface() const
{
    return interface;
}

//------------------------------------------------------------------------------------------------

Network::BindingAddress const& Configuration::EndpointOptions::GetBinding() const
{
    return bindingAddress;
}

//------------------------------------------------------------------------------------------------

std::optional<Network::RemoteAddress> const& Configuration::EndpointOptions::GetBootstrap() const
{
    return optBootstrapAddress;
}

//------------------------------------------------------------------------------------------------

Configuration::SecurityOptions::SecurityOptions()
    : type(Security::Strategy::Invalid)
    , strategy()
    , token()
    , authority()
{
}

//------------------------------------------------------------------------------------------------

Configuration::SecurityOptions::SecurityOptions(
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

//------------------------------------------------------------------------------------------------

Configuration::Settings::Settings()
    : version(Brypt::Version)
    , details()
    , endpoints()
    , security()
{
}

//------------------------------------------------------------------------------------------------

Configuration::Settings::Settings(
    DetailsOptions const& detailsOptions,
    EndpointConfigurations const& endpointsConfigurations,
    SecurityOptions const& securityOptions)
    : details(detailsOptions)
    , endpoints(endpointsConfigurations)
    , security(securityOptions)
{
}

//------------------------------------------------------------------------------------------------

std::string const& Configuration::Settings::GetVersion() const
{
    return version;
}

//------------------------------------------------------------------------------------------------

Configuration::IdentifierOptions const& Configuration::Settings::GetIdentifierOptions() const
{
    return identifier;
}

//------------------------------------------------------------------------------------------------

Configuration::DetailsOptions const& Configuration::Settings::GetDetailsOptions() const
{
    return details;
}

//------------------------------------------------------------------------------------------------

Configuration::EndpointConfigurations const& Configuration::Settings::GetEndpointConfigurations() const
{
    return endpoints;
}

//------------------------------------------------------------------------------------------------

Configuration::SecurityOptions const& Configuration::Settings::GetSecurityOptions() const
{
     return security;
    }

//------------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultBryptFolder()
{
    std::string filepath = defaults::FallbackConfigurationFolder; // Set the filepath root to /etc/ by default

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
    filepath += DefaultBryptFolder;

    return filepath;
}

//-----------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultConfigurationFilepath()
{
    return GetDefaultBryptFolder() / DefaultConfigurationFilename; // ../config.json
}

//-----------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultPeersFilepath()
{
    return GetDefaultBryptFolder() / DefaultKnownPeersFilename; // ../peers.json
}

//-----------------------------------------------------------------------------------------------
