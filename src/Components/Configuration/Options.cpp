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

Configuration::Options::Identifier::Identifier(std::string_view _type, std::string_view _value)
    : type(_type)
    , value(_value)
    , constructed({
        .type = Type::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Identifier::operator<=>(Identifier const& other) const noexcept
{
    if (auto const result = type <=> other.type; result != std::strong_ordering::equal) { return result; }
    if (auto const result = value <=> other.value; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::operator==(Identifier const& other) const noexcept
{
    return type == other.type && value == other.value;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Identifier::Merge(Identifier& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (type.empty()) {
        type = std::move(other.type);
        constructed.type = other.constructed.type;
    }

    if (!value.has_value()) { 
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
    std::string_view _name, std::string_view _description, std::string_view _location)
    : name(_name)
    , description(_description)
    , location(_location)
{
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Details::Merge(Details& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (name.empty()) { name = std::move(other.name); }
    if (description.empty()) { description = std::move(other.description); }
    if (location.empty()) { location = std::move(other.location); }
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
    Network::Protocol _protocol,
    std::string_view _interface,
    std::string_view _binding,
    std::optional<std::string> const& _bootstrap)
    : protocol()
    , interface(_interface)
    , binding(_binding)
    , bootstrap(_bootstrap)
    , constructed({
        .protocol = _protocol
    })
{
    switch (constructed.protocol) {
        case Network::Protocol::TCP: protocol = "TCP"; break;
        case Network::Protocol::LoRa: protocol = "LoRa"; break;
        default: assert(false); break;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    std::string_view _protocol,
    std::string_view _interface,
    std::string_view _binding,
    std::optional<std::string> const& _bootstrap)
    : protocol(_protocol)
    , interface(_interface)
    , binding(_binding)
    , bootstrap(_bootstrap)
    , constructed({
        .protocol = Network::Protocol::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Endpoint::operator<=>(Endpoint const& other) const noexcept
{
    if (auto const result = protocol <=> other.protocol; result != std::strong_ordering::equal) { return result; }
    if (auto const result = interface <=> other.interface; result != std::strong_ordering::equal) { return result; }
    if (auto const result = binding <=> other.binding; result != std::strong_ordering::equal) { return result; }
    
    // First compare if each side's optional bootstrap has a value. If they don't return that result. Otherwise,
    // if that status is equal and it's because both have a value, return the comparison of the values. 
    auto result = static_cast<bool>(bootstrap) <=> static_cast<bool>(other.bootstrap);
    if (result == std::strong_ordering::equal && bootstrap && other.bootstrap) {
        return bootstrap.value() <=> other.bootstrap.value();
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::operator==(Endpoint const& other) const noexcept
{
    auto const equals = protocol == other.protocol && 
        interface == other.interface && 
        binding == other.binding &&
        static_cast<bool>(bootstrap) == static_cast<bool>(other.bootstrap);

    // If everything else is equivalant and both sides have a bootstrap, return the equality of the bootstrap values. 
    if (equals && bootstrap && other.bootstrap) { return bootstrap.value() == other.bootstrap.value(); }
    return equals;
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
        constructed.bootstrap = { constructed.protocol, *bootstrap, true, Network::RemoteAddress::Origin::Cache };
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

Configuration::Options::Security::Security(std::string_view _strategy, std::string_view _token)
    : strategy(_strategy)
    , token(_token)
    , constructed({
        .strategy = ::Security::Strategy::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Security::operator<=>(Security const& other) const noexcept
{
    if (auto const result = strategy <=> other.strategy; result != std::strong_ordering::equal) { return result; }
    if (auto const result = token <=> other.token; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Security::operator==(Security const& other) const noexcept
{
    return strategy == other.strategy && token == other.token;
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
