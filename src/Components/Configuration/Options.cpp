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
    : type()
    , value()
    , constructed({
        .type = Type::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Identifier(std::string_view type, std::string_view value)
    : type(type)
    , value(value)
    , constructed({
        .type = Type::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Identifier::Merge(Identifier& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (type.empty()) {
        type = std::move(other.type);
        constructed.type = other.constructed.type;
    }

    if (value.has_value()) { 
        value = std::move(other.value);
        constructed.value = std::move(other.constructed.value);
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::Initialize()
{
    auto const GenerateIdentifier = [this] () -> bool {
        // Generate a new Brypt Identifier and store the network representation as the value.
        constructed.value = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
        value = *constructed.value;
        return true;
    };

    // If the identifier type is Ephemeral, then a new identifier should be generated. 
    if (type == "Ephemeral") { 
        constructed.type = Type::Ephemeral;
        return GenerateIdentifier();
    }

    // If the identifier type is Persistent, the we shall attempt to use provided value.
    if (type == "Persistent") {
        constructed.type = Type::Persistent;
        // If an identifier value has been parsed, attempt to use the provided value asthe identifier. We need to check 
        // the validity of the identifier to ensure the value was properly formatted. Otherwise, one will be generated. 
        if (value) {
            constructed.value = std::make_shared<Node::Identifier const>(*value);
            if (constructed.value->IsValid()) { return true; }
            constructed.value = nullptr;
            value.reset();
        }

        return GenerateIdentifier();
    }

    return false;
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

void Configuration::Options::Details::Merge(Details& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (name.empty()) { name = std::move(other.name); }
    if (description.empty()) { description = std::move(other.description); }
    if (location.empty()) { description = std::move(other.location); }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint()
    : protocol()
    , interface()
    , binding()
    , bootstrap()
    , constructed({
        .protocol = Network::Protocol::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    std::string_view protocol, std::string_view interface, std::string_view binding)
    : protocol(protocol)
    , interface(interface)
    , binding(binding)
    , bootstrap()
    , constructed({
        .protocol = Network::Protocol::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Endpoint::Merge(Endpoint& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (protocol.empty()) { protocol = std::move(other.protocol); }

    if (binding.empty()) {
        binding = std::move(other.binding);
        constructed.binding = std::move(other.constructed.binding);
    }

    if (!bootstrap.has_value()) { 
        bootstrap = std::move(other.bootstrap);
        constructed.bootstrap = std::move(other.constructed.bootstrap);
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::Initialize()
{
    if (!protocol.empty()) { constructed.protocol = Network::ParseProtocol(protocol); }
    if (constructed.protocol == Network::Protocol::Invalid || binding.empty() || interface.empty()) { return false; }

    constructed.binding = { constructed.protocol, binding, interface };
    if (!constructed.binding.IsValid()) {
        constructed.binding = {};
        return false;
    }

    if (bootstrap && !bootstrap->empty()) {
        constructed.bootstrap = { constructed.protocol, *bootstrap, true };
        if (!constructed.bootstrap->IsValid()) { 
            constructed.bootstrap.reset();
            return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Configuration::Options::Endpoint::GetProtocol() const { return constructed.protocol; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetProtocolString() const { return protocol; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetInterface() const { return interface; }

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress const& Configuration::Options::Endpoint::GetBinding() const { return constructed.binding; }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Network::RemoteAddress> const& Configuration::Options::Endpoint::GetBootstrap() const { return constructed.bootstrap; }

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Security::Security()
    : strategy()
    , token()
    , constructed({
        .strategy = ::Security::Strategy::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Security::Security(std::string_view strategy, std::string_view token)
    : strategy(strategy)
    , token(token)
    , constructed({
        .strategy = ::Security::Strategy::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Security::Merge(Security& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (strategy.empty()) {
        strategy = std::move(other.strategy);
        constructed.strategy = other.constructed.strategy;
    }

    if (token.empty()) { token = std::move(other.token); }
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] bool Configuration::Options::Security::Initialize()
{
    constructed.strategy = ::Security::ConvertToStrategy({ strategy.data(), strategy.size() });
    if (constructed.strategy == ::Security::Strategy::Invalid) {
        strategy.clear();
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------
