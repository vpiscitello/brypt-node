//----------------------------------------------------------------------------------------------------------------------
// File: Options.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Options.hpp"
#include "Defaults.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/lexical_cast.hpp>
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <map>
#include <ranges>
#include <utility>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------
#if defined(WIN32)
#include <windows.h>
#include <shlobj.h>
#undef interface
#endif
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::string ConvertConnectionDuration(std::chrono::milliseconds const& value);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

std::filesystem::path Configuration::GetDefaultBryptFolder()
{
#if defined(WIN32)
    std::filesystem::path filepath;
    PWSTR pFetched; 
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &pFetched) == S_OK) { filepath = pFetched; }
    CoTaskMemFree(pFetched);
#else
    std::string filepath{ Defaults::FallbackConfigurationFolder }; // Set the filepath root to /etc/ by default

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
#endif

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
    // If this option set is an ephemeral and the other's is persistent, choose the other's value. Otherwise, the 
    // current values shall remain as there's no need to generate a new identifier. 
    if (constructed.type == Type::Invalid || (constructed.type == Type::Ephemeral && other.type == "Persistent")) {
        type = std::move(other.type);
        constructed.type = other.constructed.type;
        value = std::move(other.value);
        constructed.value = std::move(other.constructed.value);
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::Initialize(std::shared_ptr<spdlog::logger> const& logger)
{
    auto const GenerateIdentifier = [this, &logger] () -> bool {
        // Generate a new Brypt Identifier and store the network representation as the value.
        constructed.value = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
        value = *constructed.value;
        auto const valid = constructed.value->IsValid();
        if (!valid) { logger->warn("Failed to generate a valid brypt identifier for the node."); }
        return valid;
    };

    // If we already have a constructed value and is valid, don't regenerate the identifier. 
    if (constructed.value && constructed.value->IsValid()) { return true; }

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
            logger->warn("Detected an invalid value in the identifier.value field.");
        }

        return GenerateIdentifier();
    }

    logger->warn("Detected an invalid value in the identifier.type field.");

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

bool Configuration::Options::Details::Initialize(std::shared_ptr<spdlog::logger> const& logger)
{
    if (constexpr std::size_t limit = 64; name.size() > limit) {
        logger->warn("Detected a value exceeding the {} character limit in the details.name field.", limit);
        return false;
    }

    if (constexpr std::size_t limit = 256; name.size() > limit) {
        logger->warn("Detected a value exceeding the {} character limit in the details.description field.", limit);
        return false;
    }
    
    if (constexpr std::size_t limit = 256; name.size() > limit) {
        logger->warn("Detected a value exceeding the {} character limit in the details.location field.", limit);
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Connection::Connection()
    : timeout()
    , retry({ .limit = Defaults::ConnectionRetryLimit })
    , constructed({
        .timeout = Defaults::ConnectionTimeout,
        .interval = Defaults::ConnectionRetryInterval
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Connection::Connection(
    std::chrono::milliseconds const& _timeout, std::int32_t _limit, std::chrono::milliseconds const& _interval)
    : timeout(local::ConvertConnectionDuration(_timeout))
    , retry({
        .limit = _limit,
        .interval = local::ConvertConnectionDuration(_interval)
    })
    , constructed({
        .timeout = _timeout,
        .interval = _interval
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Connection::operator<=>(Connection const& other) const noexcept
{
    if (auto const result = timeout <=> other.timeout; result != std::strong_ordering::equal) { return result; }
    if (auto const result = retry <=> other.retry; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::operator==(Connection const& other) const noexcept
{
    return timeout == other.timeout && retry == other.retry;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Connection::Merge(Connection& other)
{
    if (timeout.empty()) { 
        timeout = std::move(other.timeout);
        constructed.timeout = std::move(other.constructed.timeout);
    }

    if (retry.limit == Defaults::ConnectionRetryLimit) { retry.limit = std::move(other.retry.limit); }
    if (retry.interval.empty()) { 
        retry.interval = std::move(other.retry.interval);
        constructed.interval = std::move(other.constructed.interval);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Connection::Merge(Connection const& other)
{
    if (timeout.empty()) { 
        timeout = other.timeout;
        constructed.timeout = other.constructed.timeout;
    }

    if (retry.limit == Defaults::ConnectionRetryLimit) { retry.limit = other.retry.limit; }
    if (retry.interval.empty()) { 
        retry.interval = other.retry.interval;
        constructed.interval = other.constructed.interval;
    }
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::Initialize(std::shared_ptr<spdlog::logger> const& logger)
{
    constexpr auto ConstructDuration = [] (std::string const& value) -> std::optional<std::chrono::milliseconds> {
        enum class Duration : std::uint32_t { Milliseconds, Seconds, Minutes, Invalid };
        constexpr auto ParseDuration = [] (std::string const& value) -> std::pair<Duration, std::size_t> {
            static std::unordered_map<Duration, std::string> const durations = { 
                { Duration::Milliseconds, "ms" },
                { Duration::Seconds, "s" },
                { Duration::Minutes, "min" }
            };

            // Find the first instance of a non-numeric character. 
            auto const first = std::ranges::find_if(value, [] (char c) { return std::isalpha(c); });
            if (first == value.end()) { return { Duration::Invalid, 0 }; } // No postfix was found. 

            // Attempt to map the postfix to a duration entry. 
            std::string_view const postfix = { first, value.end() };
            auto const entry = std::ranges::find_if(durations | std::views::values, [&] (std::string_view const& value) { 
                return postfix == value;
            });

            // If no mapping could be found, indicate an error has occured. 
            if (entry.base() == durations.end()) { return { Duration::Invalid, 0 }; }

            // Returns the type of entry and the number of characters in the numeric value. 
            return { entry.base()->first, value.size() - postfix.size() };
        };
        
        try {
            auto const [duration, size] = ParseDuration(value); // Get the duration and end of the numeric value.
            if (duration == Duration::Invalid || size == 0) { return {}; } // No valid duration was found.

            // Attempt to cast the provided value to a number and then adjust it to milliseconds. 
            auto count = boost::lexical_cast<std::uint32_t>(value.c_str(), size);
            switch (duration) {
                case Duration::Minutes: count *= 60; [[fallthrough]];
                case Duration::Seconds: count *= 1000; [[fallthrough]];
                case Duration::Milliseconds: break; 
                case Duration::Invalid: assert(false); return {};
            }

            return std::chrono::milliseconds{ count };
        } catch(boost::bad_lexical_cast& exception) {
            return {};
        }
    };

    if (timeout.empty()) {
        assert(constructed.timeout == Defaults::ConnectionTimeout);
        timeout = local::ConvertConnectionDuration(constructed.timeout);
    } else {
        auto const optConstructedTimeout = ConstructDuration(timeout);
        if (!optConstructedTimeout) {
            logger->warn("Detected an invalid value in the network.connection.timeout field.");
            return false;
        }
        constructed.timeout = *optConstructedTimeout;
    }

    if (retry.limit < 0) {
        logger->warn("Detected a negative value in the network.connection.retry.limit field.");
        return false;
    }

    if (retry.interval.empty()) {
        assert(constructed.interval == Defaults::ConnectionRetryInterval);
        retry.interval = local::ConvertConnectionDuration(constructed.interval);
    } else {
        auto const optConstructedInterval = ConstructDuration(retry.interval);
        if (!optConstructedInterval) { 
            logger->warn("Detected an invalid value in the network.connection.retry.interval field.");
            return false;
        }
        constructed.interval = *optConstructedInterval;
    }

    return true; 
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Connection::GetTimeout() const
{
    return constructed.timeout;
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Options::Connection::GetRetryLimit() const
{
    return retry.limit;
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Connection::GetRetryInterval() const
{
    return constructed.interval;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::SetTimeout(std::chrono::milliseconds const& value)
{
    if (value > std::chrono::hours{ 24 }) { return false; } // An arbitrary, but still unrestrictive limit (1440m);
    if (timeout = local::ConvertConnectionDuration(value); timeout.empty()) { return false; }
    constructed.timeout = value;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::SetRetryLimit(std::int32_t value)
{
    if (!std::in_range<std::uint32_t>(value)) { return false; } // Currently, we only prevent negative values. 
    retry.limit = value;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::SetRetryInterval(std::chrono::milliseconds const& value)
{
    if (value > std::chrono::hours{ 24 }) { return false; } // An arbitrary, but still unrestrictive limit (1440m);
    if (retry.interval = local::ConvertConnectionDuration(value); retry.interval.empty()) { return false; }
    constructed.interval = value;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------


std::strong_ordering Configuration::Options::Connection::Retry::operator<=>(Connection::Retry const& other) const noexcept
{
    if (auto const result = limit <=> other.limit; result != std::strong_ordering::equal) { return result; }
    if (auto const result = interval <=> other.interval; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::Retry::operator==(Connection::Retry const& other) const noexcept
{
    return limit == other.limit && interval == other.interval;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint()
    : protocol()
    , interface()
    , binding()
    , bootstrap()
    , constructed({
        .protocol = ::Network::Protocol::Invalid
    })
    , transient()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    ::Network::Protocol _protocol,
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
    , transient({
        .useBootstraps = true
    })
{
    switch (constructed.protocol) {
        case ::Network::Protocol::TCP: protocol = "TCP"; break;
        case ::Network::Protocol::LoRa: protocol = "LoRa"; break;
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
        .protocol = ::Network::Protocol::Invalid
    })
    , transient({
        .useBootstraps = true
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Endpoint::operator<=>(Endpoint const& other) const noexcept
{
    if (auto const result = protocol <=> other.protocol; result != std::strong_ordering::equal) { return result; }
    if (auto const result = interface <=> other.interface; result != std::strong_ordering::equal) { return result; }
    if (auto const result = binding <=> other.binding; result != std::strong_ordering::equal) { return result; }
    if (auto const result = bootstrap <=> other.bootstrap; result != std::strong_ordering::equal) { return result; }
    if (auto const result = connection <=> other.connection; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::operator==(Endpoint const& other) const noexcept
{
    return protocol == other.protocol && interface == other.interface &&  binding == other.binding &&
        bootstrap == other.bootstrap && connection == other.connection;
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

    if (connection.has_value() && other.connection.has_value()) { connection->Merge(*other.connection); }
    else if (!connection.has_value()) { connection = std::move(other.connection); }
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::Initialize(Runtime runtime, std::shared_ptr<spdlog::logger> const& logger)
{
    if (!protocol.empty()) { constructed.protocol = ::Network::ParseProtocol(protocol); }
    if (constructed.protocol == ::Network::Protocol::Invalid || binding.empty() || interface.empty()) { return false; }

    constructed.binding = { constructed.protocol, binding, interface };
    if (!constructed.binding.IsValid()) {
        logger->warn("Detected an invalid address in an endpoint.binding field.");
        constructed.binding = {};
        return false;
    }

    if (bootstrap && !bootstrap->empty()) {
        constructed.bootstrap = { constructed.protocol, *bootstrap, true, ::Network::RemoteAddress::Origin::User };
        if (!constructed.bootstrap->IsValid()) { 
            logger->warn("Detected an invalid address in an endpoint.bootstrap field.");
            constructed.bootstrap.reset();
            return false;
        }
    }

    if (connection && !connection->Initialize(logger)) { return false; }

    transient.useBootstraps = runtime.useBootstraps;

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

std::optional<Network::RemoteAddress> const& Configuration::Options::Endpoint::GetBootstrap() const
{
    return constructed.bootstrap;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::UseBootstraps() const { return transient.useBootstraps; }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Endpoint::GetConnectionTimeout() const
{
    if (!connection) { return Defaults::ConnectionTimeout; }
    return connection->GetTimeout();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Options::Endpoint::GetConnectionRetryLimit() const
{ 
    if (!connection) { return Defaults::ConnectionRetryLimit; }
    return connection->GetRetryLimit();
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Endpoint::GetConnectionRetryInterval() const
{
    if (!connection) { return Defaults::ConnectionRetryInterval; }
    return connection->GetRetryInterval();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionTimeout(std::chrono::milliseconds const& timeout)
{
    if (!connection) { connection = Connection{}; }
    return connection->SetTimeout(timeout);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionRetryLimit(std::int32_t limit)
{
    if (!connection) { connection = Connection{}; }
    return connection->SetRetryLimit(limit);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionRetryInterval(std::chrono::milliseconds const& interval)
{
    if (!connection) { connection = Connection{}; }
    return connection->SetRetryInterval(interval);
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Endpoint::SetConnectionOptions(Connection const& options)
{
    if (connection) { connection->Merge(options); } else { connection = options; }
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Configuration::Options::Endpoint Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(
    std::string_view _interface, std::string_view _binding)
{
    Endpoint options;
    options.protocol = ::Network::TestScheme;
    options.interface = _interface;
    options.binding = _binding;
    options.constructed.protocol = ::Network::Protocol::Test;
    options.constructed.binding = ::Network::BindingAddress::CreateTestAddress<InvokeContext::Test>(_binding, _interface);
    options.transient.useBootstraps = true;
    return options;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::Network()
    : endpoints()
    , connection()
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Network::operator<=>(Network const& other) const noexcept
{
    if (auto const result = endpoints <=> other.endpoints; result != std::strong_ordering::equal) { return result; }
    if (auto const result = connection <=> other.connection; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::operator==(Network const& other) const noexcept
{
    return endpoints == other.endpoints && connection == other.connection;;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Network::Merge(Network& other)
{
    connection.Merge(other.connection);

    std::ranges::for_each(other.endpoints, [this] (Endpoint& options) {
        auto const itr = std::ranges::find_if(endpoints, [&options] (Endpoint const& existing) {
            return options.binding == existing.binding;
        });
        if (itr == endpoints.end()) { endpoints.emplace_back(options); } else { itr->Merge(options); }
    });
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::Initialize(Runtime runtime, std::shared_ptr<spdlog::logger> const& logger)
{
    if (!connection.Initialize(logger)) { return false; }

    return std::ranges::all_of(endpoints, [&] (Endpoint& endpoint) {
        endpoint.SetConnectionOptions(connection); // Add or merge the global connection options for the endpoint. 
        bool const success = endpoint.Initialize(runtime, logger);
        if (!success) { logger->warn("Detected an invalid configuration for a {} endpoint", endpoint.protocol); }
        return success;
    });
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoints const& Configuration::Options::Network::GetEndpoints() const { return endpoints; }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Network::GetConnectionTimeout() const
{
    return connection.GetTimeout();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Options::Network::GetConnectionRetryLimit() const { return connection.GetRetryLimit(); }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Network::GetConnectionRetryInterval() const
{
    return connection.GetRetryInterval();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::SetConnectionTimeout(std::chrono::milliseconds const& timeout)
{
    return connection.SetTimeout(timeout);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::SetConnectionRetryLimit(std::int32_t limit)
{
    return connection.SetRetryLimit(limit);
}

//----------------------------------------------------------------------------------------------------------------------

bool  Configuration::Options::Network::SetConnectionRetryInterval(std::chrono::milliseconds const& interval)
{
    return connection.SetRetryInterval(interval);
}

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

    if (!token.has_value()) { token = std::move(other.token); }
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] bool Configuration::Options::Security::Initialize(std::shared_ptr<spdlog::logger> const& logger)
{
    constructed.strategy = ::Security::ConvertToStrategy({ strategy.data(), strategy.size() });
    if (constructed.strategy == ::Security::Strategy::Invalid) {
        logger->warn("Detected an invalid value in an security.strategy field.");
        strategy.clear();
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::ConvertConnectionDuration(std::chrono::milliseconds const& value)
{
    assert(value <= std::chrono::hours{ 24 });

    using namespace std::chrono_literals;
    static std::map<std::chrono::milliseconds, std::string> const durations = { 
        { 0ms, "ms" }, // 0 - 999 milliseconds 
        { 1'000ms, "s" }, // 1 - 59 seconds
        { 60'000ms, "min" }, // 1 - ? minutes
        { std::chrono::milliseconds::max(), "?" } // Ensures we get a proper mapping for minutes. 
    };

    // Get the first value that is less than or equal to the provided value, start the seach from the largest bound. 
    auto const bounds = durations | std::views::reverse | std::views::keys;
    auto const bound = std::ranges::find_if(bounds, [&] (auto const& duration) { return value >= duration;  });
    assert(bound.base() != durations.rbegin()); // The provided value should never greater than the millisecond max. 
    auto const divisor = std::max((*bound).count(), std::int64_t(1)); // Binc the zero bound to one. 
    auto const& postfix = bound.base()->second;
    return std::to_string(value.count() / divisor).append(postfix);
}

//----------------------------------------------------------------------------------------------------------------------
