//----------------------------------------------------------------------------------------------------------------------
// File: Options.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Options.hpp"
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace defaults {
//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path const FallbackConfigurationFolder = "/etc/";

//----------------------------------------------------------------------------------------------------------------------
} // default namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultConfigurationFilepath()
{
    return GetDefaultBryptFolder() / DefaultConfigurationFilename; // ../config.json
}

//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultBootstrapFilepath()
{
    return GetDefaultBryptFolder() / DefaultBootstrapFilename; // ../bootstrap.json
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Identifier()
    : value()
    , type()
    , spConstructedValue()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Identifier(std::string_view type)
    : value()
    , type(type)
    , spConstructedValue()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Identifier(std::string_view value, std::string_view type)
    : value(value)
    , type(type)
    , spConstructedValue()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Details::Details()
    : name()
    , description()
    , location()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Details::Details(
    std::string_view name, std::string_view description, std::string_view location)
    : name(name)
    , description(description)
    , location(location)
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint()
    : type(Network::Protocol::Invalid)
    , protocol()
    , interface()
    , binding()
    , bootstrap()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    std::string_view protocol, std::string_view interface, std::string_view binding)
    : type(Network::ParseProtocol({protocol.data(), protocol.size()}))
    , protocol(protocol)
    , interface(interface)
    , binding(binding)
    , bootstrap()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    Network::Protocol type, std::string_view interface, std::string_view binding)
    : type(type)
    , protocol(Network::ProtocolToString(type))
    , interface(interface)
    , binding(binding)
    , bootstrap()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::Initialize()
{
    if (type == Network::Protocol::Invalid && !protocol.empty()) {
        type = Network::ParseProtocol(protocol);
    }

    if (protocol.empty()) { Network::ProtocolToString(type); }

    if (bindingAddress.IsValid()) { return true; }
    if (type == Network::Protocol::Invalid || binding.empty() || interface.empty()) { return false; }

    bindingAddress = { type, binding, interface };
    if (!bindingAddress.IsValid()) { return false; }

    if (optBootstrapAddress && optBootstrapAddress->IsValid()) { return true; }
    if (bootstrap && !bootstrap->empty()) {
        optBootstrapAddress = { type, *bootstrap, true };
        if (!optBootstrapAddress->IsValid()) { return false; }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Configuration::Options::Endpoint::GetProtocol() const
{
    return type;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetProtocolName() const
{
    return protocol;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetInterface() const
{
    return interface;
}

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress const& Configuration::Options::Endpoint::GetBinding() const
{
    return bindingAddress;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Network::RemoteAddress> const& Configuration::Options::Endpoint::GetBootstrap() const
{
    return optBootstrapAddress;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Security::Security()
    : type(::Security::Strategy::Invalid)
    , strategy()
    , token()
    , authority()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Security::Security(
    std::string_view strategy, std::string_view token, std::string_view authority)
    : type(::Security::ConvertToStrategy({strategy.data(), strategy.size()}))
    , strategy(strategy)
    , token(token)
    , authority(authority)
{
}

//----------------------------------------------------------------------------------------------------------------------
