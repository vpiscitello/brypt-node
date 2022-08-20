//----------------------------------------------------------------------------------------------------------------------
// File: Options.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Options.hpp"
#include "Defaults.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/json.hpp>
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
namespace symbols {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Binding = "binding";
constexpr std::string_view Bootstrap = "bootstrap";
constexpr std::string_view Description = "description";
constexpr std::string_view Endpoints = "endpoints";
constexpr std::string_view Interface = "interface";
constexpr std::string_view Interval = "interval";
constexpr std::string_view Limit = "limit";
constexpr std::string_view Location = "location";
constexpr std::string_view Name = "name";
constexpr std::string_view Protocol = "protocol";
constexpr std::string_view Retry = "retry";
constexpr std::string_view Strategy = "strategy";
constexpr std::string_view Timeout = "timeout";
constexpr std::string_view Token = "token";
constexpr std::string_view Type = "type";
constexpr std::string_view Value = "value";

//----------------------------------------------------------------------------------------------------------------------
} // symbols namespace
//----------------------------------------------------------------------------------------------------------------------
namespace allowable {
//----------------------------------------------------------------------------------------------------------------------

std::array<std::string_view, 2> IdentifierTypes = { "Ephemeral", "Persistent" };
std::array<std::string_view, 2> EndpointTypes = { "TCP" };
std::array<std::string_view, 1> StrategyTypes = { "PQNISTL3" };

template <typename ArrayType, std::size_t ArraySize>
std::optional<std::string> IfAllowableGetValue(
    std::array<ArrayType, ArraySize> const& values,
    std::string_view value)
{
    if (value.empty()) { return {}; }

    auto const found = std::ranges::find_if(values, [&value] (std::string_view const& entry) { 
        return (boost::iequals(value, entry)); 
    });

    if (found == values.end()) { return {}; }
    return std::string(found->begin(), found->end());
}

template <typename ArrayType, std::size_t ArraySize>
void OutputValues(std::array<ArrayType, ArraySize> const& values)
{
    std::cout << "[";
    for (std::uint32_t idx = 0; idx < values.size(); ++idx) {
        std::cout << "\"" << values[idx] << "\"";
        if (idx != values.size() - 1) { std::cout << ", "; }
    }
    std::cout << "]" << std::endl;
}

//----------------------------------------------------------------------------------------------------------------------
} // allowable namespace
//----------------------------------------------------------------------------------------------------------------------
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
    : m_type()
    , m_optValue()
    , m_constructed({
        .type = Type::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Identifier(std::string_view type, std::string_view value)
    : m_type(type)
    , m_optValue(value)
    , m_constructed({
        .type = Type::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Identifier::operator<=>(Identifier const& other) const noexcept
{
    if (auto const result = m_type <=> other.m_type; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optValue <=> other.m_optValue; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::operator==(Identifier const& other) const noexcept
{
    return m_type == other.m_type && m_optValue == other.m_optValue;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Identifier::Merge(Identifier& other)
{
    // If this option set is an ephemeral and the other's is persistent, choose the other's value. Otherwise, the 
    // current values shall remain as there's no need to generate a new identifier. 
    if (m_constructed.type == Type::Invalid || (m_constructed.type == Type::Ephemeral && other.m_type == "Persistent")) {
        m_type = std::move(other.m_type);
        m_constructed.type = other.m_constructed.type;

        m_optValue = std::move(other.m_optValue);
        m_constructed.value = std::move(other.m_constructed.value);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Identifier::Merge(boost::json::object const& json)
{
    // JSON Schema:
    // "identifier": {
    //     "type": String,
    //     "value": Optional String
    // },

    auto const type = json.at(symbols::Type).as_string();
    if (m_constructed.type == Type::Invalid || (m_constructed.type == Type::Ephemeral && type == "Persistent")) {
        m_type = type;
        if (auto itr = json.find(symbols::Value); itr != json.end()) {
            m_optValue = itr->value().as_string();
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Identifier::Write(boost::json::object& json) const
{
    boost::json::object group;
    group[symbols::Type] = m_type;
    if (m_constructed.type == Type::Persistent) {
        group[symbols::Value] = static_cast<std::string const&>(*m_constructed.value);
    }
    json.emplace(Symbol, std::move(group));
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::Initialize(std::shared_ptr<spdlog::logger> const& logger)
{
    auto const GenerateIdentifier = [this, &logger] () -> bool {
        // Generate a new Brypt Identifier and store the network representation as the value.
        m_constructed.value = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
        m_optValue = *m_constructed.value;
        auto const valid = m_constructed.value->IsValid();
        if (!valid) { logger->warn("Failed to generate a valid brypt identifier for the node."); }
        return valid;
    };

    // If the identifier type is Ephemeral, then a new identifier should be generated. 
    if (m_type == "Ephemeral") {
        m_constructed.type = Type::Ephemeral;
        
        // If we already have a constructed value and is valid, don't regenerate the identifier. 
        if (m_constructed.value && m_constructed.value->IsValid()) { return true; }

        return GenerateIdentifier();
    }

    // If the identifier type is Persistent, the we shall attempt to use provided value.
    if (m_type == "Persistent") {
        m_constructed.type = Type::Persistent;
        // If an identifier value has been parsed, attempt to use the provided value asthe identifier. We need to check 
        // the validity of the identifier to ensure the value was properly formatted. Otherwise, one will be generated. 
        if (m_optValue) {
            m_constructed.value = std::make_shared<Node::Identifier const>(*m_optValue);
            if (m_constructed.value->IsValid()) { return true; }
            m_constructed.value = nullptr;
            m_optValue.reset();
            logger->warn("Detected an invalid value in the identifier.value field.");
        }

        return GenerateIdentifier();
    }

    logger->warn("Detected an invalid value in the identifier.type field.");

    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::IsAllowable() const
{
    if (m_type.empty()) { return false; }
    return allowable::IfAllowableGetValue(allowable::IdentifierTypes, m_type).has_value();
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Type Configuration::Options::Identifier::GetType() const
{
    return m_constructed.type;
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Configuration::Options::Identifier::GetValue() const
{
    return m_constructed.value;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::SetIdentifier(
    Type type, bool& changed, std::shared_ptr<spdlog::logger> const& logger)
{
    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    // Additionally, setting the type should always cause a change in the identifier. 
    changed = true;

    m_constructed.type = type;
    switch (type) {
        case Options::Identifier::Type::Ephemeral: m_type = "Ephemeral"; break;
        case Options::Identifier::Type::Persistent: m_type = "Persistent"; break;
        default: assert(false); break;
    }

    // Currently, changes  to the identifier type will always update the actual identifier. 
    m_optValue.reset();
    m_constructed.value.reset();

    return Initialize(logger);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Details::Details()
    : m_name()
    , m_description()
    , m_location()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Details::Details(
    std::string_view name, std::string_view description, std::string_view location)
    : m_name(name)
    , m_description(description)
    , m_location(location)
{
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Details::Merge(Details& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (m_name.empty()) { m_name = std::move(other.m_name); }
    if (m_description.empty()) { m_description = std::move(other.m_description); }
    if (m_location.empty()) { m_location = std::move(other.m_location); }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Details::Merge(boost::json::object const& json)
{
    // JSON Schema:
    // "details": {
    //     "name": Optional String,
    //     "description": Optional String,
    //     "location": Optional String
    // },

    if (m_name.empty()) {
        if (auto itr = json.find(symbols::Name); itr != json.end()) {
            m_name = itr->value().as_string();
        }
    }

    if (m_description.empty()) {
        if (auto const itr = json.find(symbols::Description); itr != json.end()) {
            m_description = itr->value().as_string();
        }
    }

    if (m_location.empty()) {
        if (auto itr = json.find(symbols::Location); itr != json.end()) {
            m_location = itr->value().as_string();
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Details::Write(boost::json::object& json) const
{
    boost::json::object group;
    if (!m_name.empty()) { group[symbols::Name] = m_name; }
    if (!m_description.empty()) { group[symbols::Description] = m_description; }
    if (!m_location.empty()) { group[symbols::Location] = m_location; }
    if (!group.empty()) { json.emplace(Symbol, std::move(group)); }
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Details::Initialize(std::shared_ptr<spdlog::logger> const& logger)
{
    if (constexpr std::size_t limit = 64; m_name.size() > limit) {
        logger->warn("Detected a value exceeding the {} character limit in the details.name field.", limit);
        return false;
    }

    if (constexpr std::size_t limit = 256; m_description.size() > limit) {
        logger->warn("Detected a value exceeding the {} character limit in the details.description field.", limit);
        return false;
    }
    
    if (constexpr std::size_t limit = 256; m_location.size() > limit) {
        logger->warn("Detected a value exceeding the {} character limit in the details.location field.", limit);
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Details::GetName() const { return m_name; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Details::GetDescription() const { return m_description; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Details::GetLocation() const { return m_location; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Details::SetName(std::string_view const& name, bool& changed)
{
    constexpr std::size_t limit = 64;
    if (name.size() > limit) { return false; }

    if (name != m_name) {
        m_name = name;
        changed = true;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Details::SetDescription(std::string_view const& description, bool& changed)
{
    constexpr std::size_t limit = 256;
    if (description.size() > limit) { return false; }

    if (description != m_description) {
        m_description = description;
        changed = true;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Details::SetLocation(std::string_view const& location, bool& changed) 
{
    constexpr std::size_t limit = 256;
    if (location.size() > limit) { return false; }

    if (location != m_location) {
        m_location = location;
        changed = true;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Connection::Connection()
    : m_timeout()
    , m_retry({ .limit = Defaults::ConnectionRetryLimit })
    , m_constructed({
        .timeout = Defaults::ConnectionTimeout,
        .interval = Defaults::ConnectionRetryInterval
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Connection::Connection(
    std::chrono::milliseconds const& timeout, std::int32_t limit, std::chrono::milliseconds const& interval)
    : m_timeout(local::ConvertConnectionDuration(timeout))
    , m_retry({
        .limit = limit,
        .interval = local::ConvertConnectionDuration(interval)
    })
    , m_constructed({
        .timeout = timeout,
        .interval = interval
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Connection::operator<=>(Connection const& other) const noexcept
{
    if (auto const result = m_timeout <=> other.m_timeout; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_retry <=> other.m_retry; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::operator==(Connection const& other) const noexcept
{
    return m_timeout == other.m_timeout && m_retry == other.m_retry;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Connection::Merge(Connection& other)
{
    if (m_timeout.empty()) {
        m_timeout = std::move(other.m_timeout);
        m_constructed.timeout = std::move(other.m_constructed.timeout);
    }

    if (m_retry.limit == Defaults::ConnectionRetryLimit) { m_retry.limit = std::move(other.m_retry.limit); }
    if (m_retry.interval.empty()) {
        m_retry.interval = std::move(other.m_retry.interval);
        m_constructed.interval = std::move(other.m_constructed.interval);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Connection::Merge(Connection const& other)
{
    if (m_timeout.empty()) {
        m_timeout = other.m_timeout;
        m_constructed.timeout = other.m_constructed.timeout;
    }

    if (m_retry.limit == Defaults::ConnectionRetryLimit) { m_retry.limit = other.m_retry.limit; }
    if (m_retry.interval.empty()) { 
        m_retry.interval = other.m_retry.interval;
        m_constructed.interval = other.m_constructed.interval;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Connection::Merge(boost::json::object const& json)
{
    // JSON Schema:
    // "connection": {
    //     "timeout": Optional String,
    //     "retry": {
    //       "limit": Optional Integer,
    //       "interval": Optional String
    //   },
    // },

    if (m_timeout.empty()) {
        if (auto const itr = json.find(symbols::Timeout); itr != json.end()) {
            m_timeout = itr->value().as_string();
        }
    }
    
    if (auto const itr = json.find(symbols::Retry); itr != json.end()) {
        auto const& retry = itr->value().as_object();
        if (m_retry.limit == Defaults::ConnectionRetryLimit) {
            if (auto const jtr = retry.find(symbols::Limit); jtr != retry.end()) {
                auto const& value = jtr->value().as_int64();
                if (std::in_range<std::int32_t>(value)) {
                    m_retry.limit = static_cast<std::int32_t>(value);
                }
            }
        }

        if (m_retry.interval.empty()) {
            if (auto const jtr = retry.find(symbols::Interval); jtr != retry.end()) {
                m_retry.interval = jtr->value().as_string();
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Connection::Write(
    boost::json::object& json, GlobalOptionsReference optGlobalConnectionOptions) const
{
    boost::json::object group;

    if (m_constructed.timeout != Defaults::ConnectionTimeout) {
        if (!optGlobalConnectionOptions || m_constructed.timeout != optGlobalConnectionOptions->get().GetTimeout()) {
            group[symbols::Timeout] = m_timeout; 
        }
    }

    {
        boost::json::object retry;
        if (m_retry.limit != Defaults::ConnectionRetryLimit) { 
            if (!optGlobalConnectionOptions || m_retry.limit != optGlobalConnectionOptions->get().GetRetryLimit()) {
                retry[symbols::Limit] = m_retry.limit;
            }
        }
        
        if (m_constructed.interval != Defaults::ConnectionRetryInterval) {
            if (!optGlobalConnectionOptions || m_constructed.interval != optGlobalConnectionOptions->get().GetRetryInterval()) {
                retry[symbols::Interval] = m_retry.interval;
            }
        }
        
        if (!retry.empty()) { group.emplace(symbols::Retry, std::move(retry)); }
    }

    if (!group.empty()) { json.emplace(Symbol, std::move(group)); }
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

    if (m_timeout.empty()) {
        assert(m_constructed.timeout == Defaults::ConnectionTimeout);
        m_timeout = local::ConvertConnectionDuration(m_constructed.timeout);
    } else {
        auto const optConstructedTimeout = ConstructDuration(m_timeout);
        if (!optConstructedTimeout) {
            logger->warn("Detected an invalid value in the network.connection.timeout field.");
            return false;
        }
        m_constructed.timeout = *optConstructedTimeout;
    }

    if (m_retry.limit < 0) {
        logger->warn("Detected a negative value in the network.connection.retry.limit field.");
        return false;
    }

    if (m_retry.interval.empty()) {
        assert(m_constructed.interval == Defaults::ConnectionRetryInterval);
        m_retry.interval = local::ConvertConnectionDuration(m_constructed.interval);
    } else {
        auto const optConstructedInterval = ConstructDuration(m_retry.interval);
        if (!optConstructedInterval) { 
            logger->warn("Detected an invalid value in the network.connection.retry.interval field.");
            return false;
        }
        m_constructed.interval = *optConstructedInterval;
    }

    return true; 
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Connection::GetTimeout() const
{
    return m_constructed.timeout;
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Options::Connection::GetRetryLimit() const { return m_retry.limit; }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Connection::GetRetryInterval() const
{
    return m_constructed.interval;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::SetTimeout(std::chrono::milliseconds const& value, bool& changed)
{
    if (value > std::chrono::hours{ 24 }) { return false; } // An arbitrary, but still unrestrictive limit (1440m);
    if (m_timeout = local::ConvertConnectionDuration(value); m_timeout.empty()) { return false; }
    m_constructed.timeout = value;
    changed = true;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::SetRetryLimit(std::int32_t value, bool& changed)
{
    if (!std::in_range<std::uint32_t>(value)) { return false; } // Currently, we only prevent negative values. 
    m_retry.limit = value;
    changed = true;
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::SetRetryInterval(std::chrono::milliseconds const& value, bool& changed)
{
    if (value > std::chrono::hours{ 24 }) { return false; } // An arbitrary, but still unrestrictive limit (1440m);
    if (m_retry.interval = local::ConvertConnectionDuration(value); m_retry.interval.empty()) { return false; }
    m_constructed.interval = value;
    changed = true;
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
    : m_protocol()
    , m_interface()
    , m_binding()
    , m_optBootstrap()
    , m_constructed({
        .protocol = ::Network::Protocol::Invalid
    })
    , m_transient()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    ::Network::Protocol protocol,
    std::string_view interface,
    std::string_view binding,
    std::optional<std::string> const& bootstrap)
    : m_protocol()
    , m_interface(interface)
    , m_binding(binding)
    , m_optBootstrap(bootstrap)
    , m_constructed({
        .protocol = protocol
    })
    , m_transient({
        .useBootstraps = true
    })
{
    switch (m_constructed.protocol) {
        case ::Network::Protocol::TCP: m_protocol = "TCP"; break;
        case ::Network::Protocol::LoRa: m_protocol = "LoRa"; break;
        default: assert(false); break;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    std::string_view protocol,
    std::string_view interface,
    std::string_view binding,
    std::optional<std::string> const& bootstrap)
    : m_protocol(protocol)
    , m_interface(interface)
    , m_binding(binding)
    , m_optBootstrap(bootstrap)
    , m_optConnection()
    , m_constructed({
        .protocol = ::Network::Protocol::Invalid
    })
    , m_transient({
        .useBootstraps = true
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(boost::json::object const& json)
    : m_protocol(json.at(symbols::Protocol).as_string())
    , m_interface(json.at(symbols::Interface).as_string())
    , m_binding(json.at(symbols::Binding).as_string())
    , m_optBootstrap()
    , m_optConnection()
    , m_constructed({
        .protocol = ::Network::Protocol::Invalid
    })
    , m_transient({
        .useBootstraps = true
    })
{
    if (auto const itr = json.find(symbols::Bootstrap); itr != json.end()) {
        m_optBootstrap = itr->value().as_string();
    }
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Endpoint::operator<=>(Endpoint const& other) const noexcept
{
    if (auto const result = m_protocol <=> other.m_protocol; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_interface <=> other.m_interface; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_binding <=> other.m_binding; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optBootstrap <=> other.m_optBootstrap; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optConnection <=> other.m_optConnection; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::operator==(Endpoint const& other) const noexcept
{
    return m_protocol == other.m_protocol && m_interface == other.m_interface && m_binding == other.m_binding &&
        m_optBootstrap == other.m_optBootstrap && m_optConnection == other.m_optConnection;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Endpoint::Merge(Endpoint& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (m_protocol.empty()) { m_protocol = std::move(other.m_protocol); }

    if (m_interface.empty()) { m_interface = std::move(other.m_interface); }

    if (m_binding.empty()) {
        m_binding = std::move(other.m_binding);
        m_constructed.binding = std::move(other.m_constructed.binding);
    }

    if (!m_optBootstrap.has_value()) {
        m_optBootstrap = std::move(other.m_optBootstrap);
        m_constructed.bootstrap = std::move(other.m_constructed.bootstrap);
    }

    if (m_optConnection.has_value() && other.m_optConnection.has_value()) {
        m_optConnection->Merge(*other.m_optConnection);
    } else if (!m_optConnection.has_value()) {
        m_optConnection = std::move(other.m_optConnection);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Endpoint::Merge(boost::json::object const& json)
{
    // JSON Schema:
    // "endpoints": [{
    //     "protocol": String,
    //     "interface": String,
    //     "binding": String,
    //     "bootstrap": Optional String,
    //     "connection": Optional Object,
    // }],

    if (m_protocol.empty()) {
        m_protocol = json.at(symbols::Protocol).as_string();
    }

    if (m_interface.empty()) {
        m_interface = json.at(symbols::Interface).as_string();
    }

    if (m_binding.empty()) {
        m_binding = json.at(symbols::Bootstrap).as_string();
    }

    if (!m_optBootstrap.has_value()) {
        if (auto const itr = json.find(symbols::Bootstrap); itr != json.end()) {
            m_optBootstrap = itr->value().as_string();
        }
    }

    if (auto const itr = json.find(Connection::Symbol); itr != json.end()) {
        if (!m_optConnection) { m_optConnection = Connection{}; }
        m_optConnection->Merge(itr->value().as_object());
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Endpoint::Write(
    boost::json::object& json, Connection::GlobalOptionsReference optGlobalConnectionOptions) const
{
    json[symbols::Protocol] = m_protocol;
    json[symbols::Interface] = m_interface;
    json[symbols::Binding] = m_binding;
    if (m_optBootstrap.has_value()) { json[symbols::Bootstrap] = *m_optBootstrap; }
    if (m_optConnection) { m_optConnection->Write(json, optGlobalConnectionOptions); }
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::Initialize(Runtime runtime, std::shared_ptr<spdlog::logger> const& logger)
{
    if (!m_protocol.empty()) { m_constructed.protocol = ::Network::ParseProtocol(m_protocol); }
    if (m_constructed.protocol == ::Network::Protocol::Invalid || m_binding.empty() || m_interface.empty()) { return false; }

    m_constructed.binding = { m_constructed.protocol, m_binding, m_interface };
    if (!m_constructed.binding.IsValid()) {
        logger->warn("Detected an invalid address in an endpoint.binding field.");
        m_constructed.binding = {};
        return false;
    }

    if (m_optBootstrap && !m_optBootstrap->empty()) {
        m_constructed.bootstrap = { m_constructed.protocol, *m_optBootstrap, true, ::Network::RemoteAddress::Origin::User };
        if (!m_constructed.bootstrap->IsValid()) { 
            logger->warn("Detected an invalid address in an endpoint.bootstrap field.");
            m_constructed.bootstrap.reset();
            return false;
        }
    }

    if (m_optConnection && !m_optConnection->Initialize(logger)) { return false; }

    m_transient.useBootstraps = runtime.useBootstraps;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Configuration::Options::Endpoint::GetProtocol() const { return m_constructed.protocol; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetProtocolString() const { return m_protocol; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetInterface() const { return m_interface; }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetBindingField() const { return m_binding; }

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress const& Configuration::Options::Endpoint::GetBinding() const { return m_constructed.binding; }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Network::RemoteAddress> const& Configuration::Options::Endpoint::GetBootstrap() const
{
    return m_constructed.bootstrap;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> const& Configuration::Options::Endpoint::GetBootstrapField() const { return m_optBootstrap; }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::UseBootstraps() const { return m_transient.useBootstraps; }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Endpoint::GetConnectionTimeout() const
{
    if (!m_optConnection) { return Defaults::ConnectionTimeout; }
    return m_optConnection->GetTimeout();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Options::Endpoint::GetConnectionRetryLimit() const
{ 
    if (!m_optConnection) { return Defaults::ConnectionRetryLimit; }
    return m_optConnection->GetRetryLimit();
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Endpoint::GetConnectionRetryInterval() const
{
    if (!m_optConnection) { return Defaults::ConnectionRetryInterval; }
    return m_optConnection->GetRetryInterval();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionTimeout(std::chrono::milliseconds const& timeout, bool& changed)
{
    if (!m_optConnection) { m_optConnection = Connection{}; }
    return m_optConnection->SetTimeout(timeout, changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionRetryLimit(std::int32_t limit, bool& changed)
{
    if (!m_optConnection) { m_optConnection = Connection{}; }
    return m_optConnection->SetRetryLimit(limit, changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionRetryInterval(std::chrono::milliseconds const& interval, bool& changed)
{
    if (!m_optConnection) { m_optConnection = Connection{}; }
    return m_optConnection->SetRetryInterval(interval, changed);
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Endpoint::UpsertConnectionOptions(Connection const& options)
{
    if (m_optConnection) { 
        m_optConnection->Merge(options);
    } else {
        m_optConnection = options;
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Configuration::Options::Endpoint Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(
    ::Network::BindingAddress const& _binding)
{
    Endpoint options;
    options.m_protocol = ::Network::TestScheme;
    options.m_interface = _binding.GetInterface();
    options.m_binding = _binding.GetAuthority();
    options.m_constructed.protocol = _binding.GetProtocol();
    options.m_constructed.binding = _binding;
    options.m_transient.useBootstraps = true;
    return options;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Configuration::Options::Endpoint Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(
    std::string_view _interface, std::string_view _binding)
{
    Endpoint options;
    options.m_protocol = ::Network::TestScheme;
    options.m_interface = _interface;
    options.m_binding = _binding;
    options.m_constructed.protocol = ::Network::Protocol::Test;
    options.m_constructed.binding = ::Network::BindingAddress::CreateTestAddress<InvokeContext::Test>(
        _binding, _interface);
    options.m_transient.useBootstraps = true;
    return options;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::Network()
    : m_endpoints()
    , m_connection()
    , m_optToken()
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Network::operator<=>(Network const& other) const noexcept
{
    if (auto const result = m_endpoints <=> other.m_endpoints; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_connection <=> other.m_connection; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optToken <=> other.m_optToken; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::operator==(Network const& other) const noexcept
{
    return m_endpoints == other.m_endpoints && m_connection == other.m_connection && m_optToken == other.m_optToken;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Network::Merge(Network& other)
{
    std::ranges::for_each(other.m_endpoints, [this] (Endpoint& options) {
        auto const itr = std::ranges::find_if(m_endpoints, [&options] (Endpoint const& existing) {
            return options.GetBinding() == existing.GetBinding();
        });
        if (itr == m_endpoints.end()) { m_endpoints.emplace_back(options); } else { itr->Merge(options); }
    });

    m_connection.Merge(other.m_connection);

    if (!m_optToken) { m_optToken = std::move(other.m_optToken); }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Network::Merge(boost::json::object const& json)
{
    // JSON Schema:
    // "network": {
    //   "endpoints": Array,
    //   "connection": Optional Object,
    //   "token": Optional String
    // },

    auto const endpoints = json.at(symbols::Endpoints).as_array();
    std::ranges::for_each(endpoints, [&] (boost::json::value const& value) {
        Endpoint options{ value.as_object() };
        auto const itr = std::ranges::find_if(m_endpoints, [&options] (Endpoint const& existing) {
            return options.GetBinding() == existing.GetBinding();
        });
        if (itr == m_endpoints.end()) { m_endpoints.emplace_back(options); } else { itr->Merge(options); }
    });


    if (auto const itr = json.find(Connection::Symbol); itr != json.end()) {
        m_connection.Merge(itr->value().as_object());
    }

    if (!m_optToken) {
        if (auto const itr = json.find(symbols::Token); itr != json.end()) {
            m_optToken = itr->value().as_string();
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Network::Write(boost::json::object& json) const
{
    boost::json::object group;

    if (!m_endpoints.empty()) {
        boost::json::array endpoints;
        std::ranges::for_each(m_endpoints, [&] (auto const& endpoint) {
            boost::json::object object;
            endpoint.Write(object, m_connection);
            endpoints.emplace_back(std::move(object));
        });
        group.emplace(symbols::Endpoints, std::move(endpoints));
    }

    m_connection.Write(group);
    if (m_optToken) { group[symbols::Token] = *m_optToken; }

    json.emplace(Symbol, std::move(group));
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::Initialize(Runtime const& runtime, std::shared_ptr<spdlog::logger> const& logger)
{
    if (!m_connection.Initialize(logger)) { return false; }

    return std::ranges::all_of(m_endpoints, [&] (Endpoint& endpoint) {
        endpoint.UpsertConnectionOptions(m_connection); // Add or merge the global connection options for the endpoint. 

        bool const success = endpoint.Initialize(runtime, logger);
        if (!success) { 
            logger->warn("Detected an invalid configuration for a {} endpoint", endpoint.GetProtocolString());
        }

        return success;
    });
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::IsAllowable(Runtime const& runtime) const
{
    // If the node is running in the foreground, the node will not be able to connect to the network when no endpoint 
    // options are defined in the configuration file. In the case of the background context, the library user is able
    // to attach endpoints any time. 
    if (runtime.context == RuntimeContext::Foreground && m_endpoints.empty()) { return false; }
    return std::ranges::all_of(m_endpoints, [] (auto const& endpoint) {
        return allowable::IfAllowableGetValue(allowable::EndpointTypes, endpoint.GetProtocolString()).has_value();
    });
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoints const& Configuration::Options::Network::GetEndpoints() const { return m_endpoints; }

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Options::Network::GetEndpoint(
    ::Network::BindingAddress const& binding) const
{
    auto const itr = std::ranges::find_if(m_endpoints, [&binding] (Endpoint const& existing) {
        return binding == existing.GetBinding();
    });

    if (itr == m_endpoints.end()) { return {}; }
    return *itr;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Options::Network::GetEndpoint(
    std::string_view const& uri) const
{
    auto const itr = std::ranges::find_if(m_endpoints, [&uri] (Endpoint const& existing) {
        return uri == existing.GetBinding().GetUri();
    });

    if (itr == m_endpoints.end()) { return {}; }
    return *itr;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Options::Network::GetEndpoint(
    ::Network::Protocol protocol, std::string_view const& binding) const
{
    auto const itr = std::ranges::find_if(m_endpoints, [&protocol, &binding] (Endpoint const& existing) {
        return protocol == existing.GetProtocol() && binding == existing.GetBindingField();
    });

    if (itr == m_endpoints.end()) { return {}; }
    return *itr;
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Network::GetConnectionTimeout() const
{
    return m_connection.GetTimeout();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Options::Network::GetConnectionRetryLimit() const { return m_connection.GetRetryLimit(); }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Network::GetConnectionRetryInterval() const
{
    return m_connection.GetRetryInterval();
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> const& Configuration::Options::Network::GetToken() const { return m_optToken; }

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Options::Network::UpsertEndpoint(
    Endpoint&& options, Runtime const& runtime, std::shared_ptr<spdlog::logger> const& logger, bool& changed)
{
    if (!options.Initialize(runtime, logger)) { return {}; } // If the provided options are invalid, return false. 

    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    changed = true;
    auto const itr = std::ranges::find_if(m_endpoints, [&options] (Options::Endpoint const& existing) {
        return options.GetBindingField() == existing.GetBindingField();
    });

    // If no matching options were found, insert the options and return the initialized values. 
    if (itr == m_endpoints.end()) { return m_endpoints.emplace_back(std::move(options)); }

    *itr = std::move(options); // Update the matched options with the new values. 
    return *itr;    
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Configuration::Options::Endpoint> Configuration::Options::Network::ExtractEndpoint(
    ::Network::BindingAddress const& binding, bool& changed)
{
    std::optional<Endpoint> optExtractedOptions;
    auto const count = std::erase_if(m_endpoints, [&] (Options::Endpoint& existing) {
        bool const found = binding == existing.GetBinding();
        if (found) { optExtractedOptions = std::move(existing); }
        return found;
    });

    changed = count == 1;
    return optExtractedOptions;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Configuration::Options::Endpoint> Configuration::Options::Network::ExtractEndpoint(
    std::string_view const& uri, bool& changed)
{
    std::optional<Options::Endpoint> optExtractedOptions;
    auto const count = std::erase_if(m_endpoints, [&] (Options::Endpoint& existing) {
        bool const found = uri == existing.GetBinding().GetUri();
        if (found) { optExtractedOptions = std::move(existing); }
        return found;
    });

    changed = count == 1;
    return optExtractedOptions;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Configuration::Options::Endpoint> Configuration::Options::Network::ExtractEndpoint(
    ::Network::Protocol protocol, std::string_view const& binding, bool& changed)
{
    std::optional<Options::Endpoint> optExtractedOptions;
    auto const count = std::erase_if(m_endpoints, [&] (Options::Endpoint& existing) {
        bool const found = protocol == existing.GetProtocol() && binding == existing.GetBindingField();
        if (found) { optExtractedOptions = std::move(existing); }
        return found;
    });

    changed = count == 1;
    return optExtractedOptions;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::SetConnectionTimeout(std::chrono::milliseconds const& timeout, bool& changed)
{
    return m_connection.SetTimeout(timeout, changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::SetConnectionRetryLimit(std::int32_t limit, bool& changed)
{
    return m_connection.SetRetryLimit(limit, changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool  Configuration::Options::Network::SetConnectionRetryInterval(std::chrono::milliseconds const& interval, bool& changed)
{
    return m_connection.SetRetryInterval(interval, changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::SetToken(std::string_view const& token, bool& changed)
{
    if (!m_optToken || *m_optToken != token) {
        m_optToken = token;
        changed = true;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Security::Security()
    : m_strategy()
    , m_constructed({
        .strategy = ::Security::Strategy::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Security::Security(std::string_view strategy)
    : m_strategy(strategy)
    , m_constructed({
        .strategy = ::Security::Strategy::Invalid
    })
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Security::operator<=>(Security const& other) const noexcept
{
    if (auto const result = m_strategy <=> other.m_strategy; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Security::operator==(Security const& other) const noexcept
{
    return m_strategy == other.m_strategy;
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Security::Merge(Security& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (m_strategy.empty()) {
        m_strategy = std::move(other.m_strategy);
        m_constructed.strategy = other.m_constructed.strategy;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Security::Merge(boost::json::object const& json)
{
    // JSON Schema:
    // "security": {
    //     "strategy": String,
    // }

    m_strategy = json.at(symbols::Strategy).as_string();
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Security::Write(boost::json::object& json) const
{
    boost::json::object group;
    group[symbols::Strategy] = m_strategy;
    json.emplace(Symbol, std::move(group));
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] bool Configuration::Options::Security::Initialize(std::shared_ptr<spdlog::logger> const& logger)
{
    m_constructed.strategy = ::Security::ConvertToStrategy({ m_strategy.data(), m_strategy.size() });
    if (m_constructed.strategy == ::Security::Strategy::Invalid) {
        logger->warn("Detected an invalid value in an security.strategy field.");
        m_strategy.clear();
        return false;
    }
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Security::IsAllowable() const
{
    return allowable::IfAllowableGetValue(allowable::StrategyTypes, m_strategy).has_value();
}

//----------------------------------------------------------------------------------------------------------------------

::Security::Strategy Configuration::Options::Security::GetStrategy() const { return m_constructed.strategy; }

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Security::SetStrategy(::Security::Strategy strategy, bool& changed)
{
    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    if (strategy != m_constructed.strategy) {
        m_constructed.strategy = strategy;
        switch (strategy) {
            case ::Security::Strategy::PQNISTL3: m_strategy = "PQNISTL3"; break;
            default: assert(false); break;
        }
        changed = true;
    }
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
