//----------------------------------------------------------------------------------------------------------------------
// File: Options.cpp 
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "Options.hpp"
#include "Defaults.hpp"
#include "SerializationErrors.hpp"
#include "Components/Security/Algorithms.hpp"
#include "Utilities/Enumerate.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/json.hpp>
#include <spdlog/spdlog.h>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <format>
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
namespace allowable {
//----------------------------------------------------------------------------------------------------------------------

using namespace Configuration;

//----------------------------------------------------------------------------------------------------------------------

static std::array<std::pair<std::string, Options::Identifier::Persistence>, 2> const PersistenceValues = { 
    std::make_pair("ephemeral", Options::Identifier::Persistence::Ephemeral),
    std::make_pair("persistent", Options::Identifier::Persistence::Persistent),
};

//----------------------------------------------------------------------------------------------------------------------

static std::array<std::pair<std::string, Network::Protocol>, 2> const ProtocolValues = { 
    std::make_pair("tcp", Network::Protocol::TCP),
    std::make_pair("test", Network::Protocol::Test)
};

//----------------------------------------------------------------------------------------------------------------------

static std::array<std::pair<std::string, Security::ConfidentialityLevel>, 3> const ConfidentialityValues = { 
    std::make_pair("low", Security::ConfidentialityLevel::Low),
    std::make_pair("medium", Security::ConfidentialityLevel::Medium),
    std::make_pair("high", Security::ConfidentialityLevel::High),
};

//----------------------------------------------------------------------------------------------------------------------

template<typename ValueType, std::size_t ArraySize>
[[nodiscard]] bool IsAllowable(
    std::array<std::pair<std::string_view, ValueType>, ArraySize> const& values,
    std::string_view value)
{
    using PairType = std::pair<std::string_view, ValueType>;

    if (value.empty()) { return {}; }

    auto const lower = value | std::ranges::views::transform([] (unsigned char c) { return std::tolower(c); });
    auto const comparison = [] (PairType const& left, PairType const& right) {
        return (left.first | std::ranges::views::transform([] (unsigned char c) { return std::tolower(c); })) < right.first;
    };
    
    return std::ranges::binary_search(values, lower, comparison);
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ValueType, std::size_t ArraySize>
[[nodiscard]] std::optional<ValueType> IfAllowableGetValue(
    std::array<std::pair<std::string, ValueType>, ArraySize> const& values,
    std::string_view value)
{
    using PairType = std::pair<std::string_view, ValueType>;

    if (value.empty()) { return {}; }

    auto const found = std::ranges::find_if(values, [&value] (PairType const& entry) { 
        return (boost::iequals(value, entry.first)); 
    });

    if (found == values.end()) { return {}; }
    return found->second;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ValueType, std::size_t ArraySize>
[[nodiscard]] std::optional<std::string> IfAllowableGetString(
    std::array<std::pair<std::string, ValueType>, ArraySize> const& values,
    ValueType value)
{
    using PairType = std::pair<std::string_view, ValueType>;

    auto const found = std::ranges::find_if(values, [&value] (PairType const& entry) { 
        return value == entry.second; 
    });

    if (found == values.end()) { return {}; }
    return std::string{ found->first.data(), found->first.size() };
}

//----------------------------------------------------------------------------------------------------------------------
} // allowable namespace
//----------------------------------------------------------------------------------------------------------------------
namespace local {
//----------------------------------------------------------------------------------------------------------------------

using namespace Configuration;

//---------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::optional<Options::Identifier::Persistence> StringToPersistence(std::string_view value);
[[nodiscard]] std::optional<std::string >StringFromPersistence(Options::Identifier::Persistence value);

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::optional<Node::SharedIdentifier> StringToSharedIdentifier(std::string_view value);
[[nodiscard]] std::optional<std::string >StringFromSharedIdentifier(Node::SharedIdentifier const& spIdentifier);

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::optional<std::chrono::milliseconds> StringToMilliseconds(std::string_view value);
[[nodiscard]] std::optional<std::string> StringFromMilliseconds(std::chrono::milliseconds const& value);

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::optional<Network::Protocol> StringToNetworkProtocol(std::string_view value);
[[nodiscard]] std::optional<std::string> StringFromNetworkProtocol(Network::Protocol const& value);

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::optional<std::string> StringFromBindingAddress(Network::BindingAddress const& value);
[[nodiscard]] std::optional<std::string> StringFromRemoteAddress(Network::RemoteAddress const& value);

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::optional<Security::ConfidentialityLevel> StringToConfidentialityLevel(std::string_view value);
[[nodiscard]] std::optional<std::string >StringFromConfidentialityLevel(Security::ConfidentialityLevel value);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
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

    // Attempt to get the $XDG_CONFIG_HOME environment variable to use as the configuration directory
    // If $XDG_CONFIG_HOME does not exist try to get the user home directory 
    auto const systemConfigHomePath = std::getenv("XDG_CONFIG_HOME");
    if (systemConfigHomePath) {
        std::size_t const length = strlen(systemConfigHomePath);
        filepath = std::string(systemConfigHomePath, length);
    } else {
        // Attempt to get the $HOME environment variable to use the the configuration directory
        // If $HOME does not exist continue with the fall configuration path
        auto const userHomePath = std::getenv("HOME");
        if (userHomePath) {
            std::size_t const length = strlen(userHomePath);
            filepath = std::string(userHomePath, length) + "/.config";
        }
    }
#endif

    // Concat /brypt/ to the configuration folder path
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
    return GetDefaultBryptFolder() / DefaultBootstrapFilename; // ../bootstraps.json
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Identifier()
    : m_persistence(
        local::StringToPersistence,
        local::StringFromPersistence,
        [] (auto const& value) { return value != Persistence::Invalid; })
    , m_optValue(
        local::StringToSharedIdentifier,
        local::StringFromSharedIdentifier,
        [] (auto const& value) { return value != nullptr; })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Identifier(std::string_view persistence, std::string_view value)
    : m_persistence(
        persistence,
        local::StringToPersistence,
        local::StringFromPersistence,
        [] (auto const& value) { return value != Persistence::Invalid; })
    , m_optValue(
        value,
        local::StringToSharedIdentifier,
        local::StringFromSharedIdentifier,
        [] (auto const& value) { return value != nullptr; })
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::operator==(Identifier const& other) const noexcept
{
    return m_persistence == other.m_persistence && m_optValue == other.m_optValue;
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Identifier::operator<=>(Identifier const& other) const noexcept
{
    if (auto const result = m_persistence <=> other.m_persistence; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optValue <=> other.m_optValue; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Identifier::Merge(boost::json::object const& json)
{
    // JSON Schema:
    // "identifier": {
    //     "type": String,
    //     "value": Optional String
    // },

    auto const itr = json.find(m_persistence.GetFieldName());
    if (itr == json.end()) {
        return { 
            StatusCode::DecodeError,
            CreateMissingFieldMessage(GetFieldName(), m_persistence.GetFieldName())
        };
    }

    if (!itr->value().is_string()) {
        return {
            StatusCode::DecodeError,
            CreateMismatchedValueTypeMessage("string", GetFieldName(), m_persistence.GetFieldName())
        };
    }

    auto const& persistence = itr->value().as_string();

    // If this option set is an ephemeral and the other's is persistent, choose the other's value. Otherwise, the 
    // current values shall remain as there's no need to generate a new identifier. 
    //if (m_constructed.type == Persistence::Invalid || (m_constructed.type == Persistence::Ephemeral && other.m_type == "Persistent")) {
    if (m_persistence.NotModified()) {
        {
            bool const success = m_persistence.SetValueFromConfig(persistence);
            if (!success) {
                return {
                    StatusCode::InputError,
                    CreateUnexpectedValueMessage(allowable::PersistenceValues, GetFieldName(), m_persistence.GetFieldName())
                };
            }
        }

        auto const GenerateIdentifier = [this] () -> DeserializationResult {
            // Generate a new Brypt Identifier and store the network representation as the value.
            auto const spIdentifier = std::make_shared<Node::Identifier const>(Node::GenerateIdentifier());
            if (auto const valid = spIdentifier->IsValid(); !valid) {
                return { StatusCode::InputError, CreateInvalidValueMessage(GetFieldName(), m_optValue.GetFieldName()) };
            }

            if (!m_optValue.SetValue(spIdentifier)) {
                return { StatusCode::InputError,  CreateUnexpectedErrorMessage(GetFieldName(), m_optValue.GetFieldName()) };
            }

            return { StatusCode::Success, "" };
        };

        // Determine how the identifier value should be processed by casing on the persistence type. 
        switch (m_persistence.GetValue()) {
            // If the identifier is ephemeral generate a new identifier and return the result.
            case Persistence::Ephemeral: {
                return GenerateIdentifier();
            } break;
            // If the identifier is persistent attempt to read a stored value, if one does not exist generate a new identifier.
            case Persistence::Persistent: {
                if (auto const itr = json.find(m_optValue.GetFieldName()); itr != json.end()) {
                    if (!itr->value().is_string()) {
                        return {
                            StatusCode::InputError,
                            CreateMismatchedValueTypeMessage("string", GetFieldName(), m_optValue.GetFieldName())
                        };
                    }

                    bool const success = m_optValue.SetValueFromConfig(itr->value().as_string());
                    if (!success) {
                        return {
                            StatusCode::InputError,
                            CreateInvalidValueMessage(GetFieldName(), m_optValue.GetFieldName())
                        };
                    }
                } else {
                    return GenerateIdentifier();
                }
            } break;
            default: assert(false); break;
        }
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::Identifier::Write(boost::json::object& json) const
{
    boost::json::object group;
    group[m_persistence.GetFieldName()] = m_persistence.GetSerializedValue();
    if (m_persistence.GetValue() == Persistence::Persistent) {
        if (m_optValue.HasValue()) {
            group[m_optValue.GetFieldName()] = m_optValue.GetSerializedValue();
        } else {
            return {
                StatusCode::InputError,
                "Attempted to write an invalid identifier value"
            };
        }
    }
    json.emplace(Symbol, std::move(group));

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::Identifier::AreOptionsAllowable() const
{
    if (m_persistence.GetValue() == Persistence::Invalid) {
        return {
            StatusCode::InputError,
            CreateUnexpectedValueMessage(allowable::PersistenceValues, GetFieldName(), m_persistence.GetFieldName())
        };
    }

    if (!m_optValue.HasValue() || !m_optValue.GetValue()->IsValid()) {
        return {
            StatusCode::InputError,
            CreateInvalidValueMessage(GetFieldName(), m_optValue.GetFieldName())
        };
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Identifier::Persistence Configuration::Options::Identifier::GetPersistence() const
{
    return m_persistence.GetValue();
}

//----------------------------------------------------------------------------------------------------------------------

Node::SharedIdentifier const& Configuration::Options::Identifier::GetValue() const
{
    return m_optValue.GetValueOrElse(nullptr);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Identifier::SetIdentifier(Persistence persistence, bool& changed)
{
    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    // Additionally, setting the type should always cause a change in the identifier. 
    changed = true;

    {
        bool const success = m_persistence.SetValue(persistence);
        if (!success) { return false; }
    }

    {
        bool const success = m_optValue.SetValue(""); // Setting the identifier to an empty string will force a new identifier to be generated.
        if (!success) { return false; }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Details::Details()
    : m_optName([] (auto const& value) { return value.size() <= NameSizeLimit; })
    , m_optDescription([] (auto const& value) { return value.size() <= DescriptionSizeLimit; })
    , m_optLocation([] (auto const& value) { return value.size() <= LocationSizeLimit; })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Details::Details(
    std::string_view name, std::string_view description, std::string_view location)
    : m_optName(name, [] (auto const& value) { return value.size() <= NameSizeLimit; })
    , m_optDescription(description, [] (auto const& value) { return value.size() <= DescriptionSizeLimit; })
    , m_optLocation(location, [] (auto const& value) { return value.size() <= LocationSizeLimit; })
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Details::operator==(Details const& other) const noexcept
{
    return m_optName == other.m_optName &&
           m_optDescription == other.m_optDescription &&
           m_optLocation == other.m_optLocation;
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Details::operator<=>(Details const& other) const noexcept
{
    if (auto const result = m_optName <=> other.m_optName; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optDescription <=> other.m_optDescription; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optLocation <=> other.m_optLocation; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Details::Merge(boost::json::object const& json)
{
    // JSON Schema:
    // "details": {
    //     "name": Optional String,
    //     "description": Optional String,
    //     "location": Optional String
    // },

    // Deserialize the name field from the details object.
    if (m_optName.NotModified()) {
        if (auto const itr = json.find(m_optName.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_optName.SetValueFromConfig(itr->value().as_string())) {
                    return { 
                        StatusCode::InputError,
                        CreateExceededCharacterLimitMessage(NameSizeLimit, GetFieldName(), m_optName.GetFieldName())
                    };
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", GetFieldName(), m_optName.GetFieldName())
                };
            }
        }
    }

    // Deserialize the description field from the details object.
    if (m_optDescription.NotModified()) {
        if (auto const itr = json.find(m_optDescription.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_optDescription.SetValueFromConfig(itr->value().as_string())) {
                    return { 
                        StatusCode::InputError,
                        CreateExceededCharacterLimitMessage(DescriptionSizeLimit, GetFieldName(), m_optName.GetFieldName())
                    };
                }
            } else {
                return {
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", GetFieldName(), m_optDescription.GetFieldName())
                };
            }
        }
    }

    // Deserialize the location field from the details object.
    if (m_optLocation.NotModified()) {
        if (auto const itr = json.find(m_optLocation.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_optLocation.SetValueFromConfig(itr->value().as_string())) {
                    return { 
                        StatusCode::InputError,
                        CreateExceededCharacterLimitMessage(LocationSizeLimit, GetFieldName(), m_optName.GetFieldName())
                    };
                }
            } else {
                return {
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", GetFieldName(), m_optLocation.GetFieldName())
                };
            }
        }
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::Details::Write(boost::json::object& json) const
{
    boost::json::object group;

    if (m_optName.HasValue()) { group[m_optName.GetFieldName()] = m_optName.GetValue(); }
    if (m_optDescription.HasValue()) { group[m_optDescription.GetFieldName()] = m_optDescription.GetValue(); }
    if (m_optLocation.HasValue()) { group[m_optLocation.GetFieldName()] = m_optLocation.GetValue(); }

    if (!group.empty()) { json.emplace(Symbol, std::move(group)); }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::Details::AreOptionsAllowable() const
{
    return { StatusCode::Success, "" }; // There are no requirements for this set of options. 
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Details::GetName() const { return m_optName.GetValueOrElse(""); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Details::GetDescription() const { return m_optDescription.GetValueOrElse(""); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Details::GetLocation() const { return m_optLocation.GetValueOrElse(""); }

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Details::SetName(std::string_view const& name, bool& changed)
{
    if (!m_optName.SetValue(name)) { return false; }
    changed = m_optName.Modified();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Details::SetDescription(std::string_view const& description, bool& changed)
{
    if (!m_optDescription.SetValue(description)) { return false; }
    changed = m_optDescription.Modified();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Details::SetLocation(std::string_view const& location, bool& changed) 
{
    if (!m_optLocation.SetValue(location)) { return false; }
    changed = m_optLocation.Modified();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Retry::Retry()
    : m_optLimit()
    , m_optInterval(
        local::StringToMilliseconds,
        local::StringFromMilliseconds,
        [] (auto const& value) { return value <= std::chrono::hours{ 24 }; })
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Retry::Retry(std::uint32_t limit, std::chrono::milliseconds const& interval)
    : m_optLimit(limit)
    , m_optInterval(
        interval,
        local::StringToMilliseconds,
        local::StringFromMilliseconds,
        [] (auto const& value) { return value <= std::chrono::hours{ 24 }; })
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Retry::operator==(Retry const& other) const noexcept
{
    return m_optLimit == other.m_optLimit &&
           m_optInterval == other.m_optInterval;
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Retry::operator<=>(Retry const& other) const noexcept
{
    if (auto const result = m_optLimit <=> other.m_optLimit; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optInterval <=> other.m_optInterval; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Retry::Merge(Retry& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (m_optLimit.NotModified()) { m_optLimit = std::move(other.m_optLimit); }
    if (m_optInterval.NotModified()) { m_optInterval = std::move(other.m_optInterval); }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Retry::Merge(
    boost::json::object const& json,
    std::string_view context,
    GlobalConnectionOptionsReference optGlobalOptions)
{
    // Deserialize the limit field from the retry object.
    if (m_optLimit.NotModified()) {
        if (auto const jtr = json.find(m_optLimit.GetFieldName()); jtr != json.end()) {
            if (jtr->value().is_int64()) {
                auto const& value = jtr->value().as_int64();
                bool const success = std::in_range<std::uint32_t>(value) &&
                    m_optLimit.SetValueFromConfig(static_cast<std::uint32_t>(value));
                if (!success) {
                    return {
                        StatusCode::InputError,
                        CreateExceededValueLimitMessage(
                            std::numeric_limits<std::uint32_t>::max(), context, GetFieldName(), m_optLimit.GetFieldName())
                    };
                }
            } else {
                return {
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("integer", context, GetFieldName(), m_optLimit.GetFieldName())
                };
            }
        } else if (optGlobalOptions) {
            [[maybe_unused]] bool const success = m_optLimit.SetValueFromConfig(optGlobalOptions->get().GetRetryLimit());
            assert(success);
        }
    }

    // Deserialize the interval field from the retry object.
    if (m_optInterval.NotModified()) {
        if (auto const itr = json.find(m_optInterval.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_optInterval.SetValueFromConfig(itr->value().as_string())) {
                    return {
                        StatusCode::InputError,
                        CreateInvalidValueMessage(context, GetFieldName(), m_optInterval.GetFieldName())
                    };
                }
            } else {
                return {
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", context, GetFieldName(), m_optInterval.GetFieldName())
                };
            }
        } else if (optGlobalOptions) {
            [[maybe_unused]] bool const success = m_optInterval.SetValueFromConfig(optGlobalOptions->get().GetRetryInterval());
            assert(success);
        }
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::Retry::Write(
    boost::json::object& json, GlobalConnectionOptionsReference optGlobalOptions) const
{
    boost::json::object group;

    bool const shouldWriteLimit =
        // Write if the limit is not equal to the default connection limit.
        !m_optLimit.WouldMatchDefault(Defaults::ConnectionRetryLimit) && 
        // And, there is no global options, or if there are, the limit is not equal to the global limit option.
        (!optGlobalOptions || !m_optLimit.WouldMatchDefault(optGlobalOptions->get().GetRetryLimit()));

    if (shouldWriteLimit) {
        group[m_optLimit.GetFieldName()] = m_optLimit.GetValue();
    }

    bool const shouldWriteInterval =
        // Write if the interval is not equal to the default connection interval.
        !m_optInterval.WouldMatchDefault(Defaults::ConnectionTimeout) && 
        // And, there is no global options, or if there are, the interval is not equal to the global interval option.
        (!optGlobalOptions || !m_optInterval.WouldMatchDefault(optGlobalOptions->get().GetTimeout()));

    if (shouldWriteInterval) {
        group[m_optInterval.GetFieldName()] = m_optInterval.GetSerializedValue();
    }
        
    if (!group.empty()) { json.emplace(GetFieldName(), std::move(group)); }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::Retry::AreOptionsAllowable() const
{
    return { StatusCode::Success, "" };  // There are no requirements for this set of options. 
}

//----------------------------------------------------------------------------------------------------------------------

std::uint32_t Configuration::Options::Retry::GetLimit() const
{
    return m_optLimit.GetValueOrElse(Defaults::ConnectionRetryLimit);
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Retry::GetInterval() const
{
    return m_optInterval.GetValueOrElse(Defaults::ConnectionRetryInterval);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Retry::SetLimit(std::int32_t value, bool& changed)
{
    if (!std::in_range<std::uint32_t>(value)) { return false; } // Currently, we only prevent negative values. 
    if (!m_optLimit.SetValue(static_cast<std::uint32_t>(value))) { return false; }
    changed = m_optLimit.Modified();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Retry::SetLimit(std::uint32_t value, bool& changed)
{
    if (!m_optLimit.SetValue(value)) { return false; }
    changed = m_optLimit.Modified();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Retry::SetInterval(std::chrono::milliseconds const& value, bool& changed)
{
    if (!m_optInterval.SetValue(value)) { return false; }
    changed = m_optInterval.Modified();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Connection::Connection()
    : m_optTimeout(
        local::StringToMilliseconds,
        local::StringFromMilliseconds,
        [] (auto const& value) { return value <= std::chrono::hours{ 24 }; })
    , m_retryOptions()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Connection::Connection(
    std::chrono::milliseconds const& timeout, std::int32_t limit, std::chrono::milliseconds const& interval)
    : m_optTimeout(
        timeout,
        local::StringToMilliseconds,
        local::StringFromMilliseconds,
        [] (auto const& value) { return value <= std::chrono::hours{ 24 }; })
    , m_retryOptions(limit, interval)
{
}

//----------------------------------------------------------------------------------------------------------------------

 bool Configuration::Options::Connection::operator==(Connection const& other) const noexcept
 {
     return m_optTimeout == other.m_optTimeout && m_retryOptions == other.m_retryOptions;
 }

//----------------------------------------------------------------------------------------------------------------------

 std::strong_ordering Configuration::Options::Connection::operator<=>(Connection const& other) const noexcept
 {
     if (auto const result = m_optTimeout <=> other.m_optTimeout; result != std::strong_ordering::equal) { return result; }
     if (auto const result = m_retryOptions <=> other.m_retryOptions; result != std::strong_ordering::equal) { return result; }
     return std::strong_ordering::equal;
 }

 //----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Connection::Merge(Connection& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (m_optTimeout.NotModified()) { m_optTimeout = std::move(other.m_optTimeout); }

    if (auto const result = m_retryOptions.Merge(other.m_retryOptions); result.first != StatusCode::Success) {
        return result;
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Connection::Merge(
    boost::json::object const& json,
    std::string_view context,
    GlobalConnectionOptionsReference optGlobalOptions)
{
    // JSON Schema:
    // "connection": {
    //     "timeout": Optional String,
    //     "retry": {
    //       "limit": Optional Integer,
    //       "interval": Optional String
    //   },
    // },

    // Deserialize the timeout field from the connection object.
    if (m_optTimeout.NotModified()) {
        if (auto const itr = json.find(m_optTimeout.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_optTimeout.SetValueFromConfig(itr->value().as_string())) {
                    return {
                        StatusCode::InputError,
                        CreateInvalidValueMessage(context, GetFieldName(), m_optTimeout.GetFieldName())
                    };
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", context, GetFieldName(), m_optTimeout.GetFieldName())
                };
            }
        } else if (optGlobalOptions) {
            [[maybe_unused]] bool const success = m_optTimeout.SetValueFromConfig(optGlobalOptions->get().GetTimeout());
            assert(success);
        }
    }
    
    // Deserialize the retry object from the connection object.
    if (auto const itr = json.find(m_retryOptions.GetFieldName()); itr != json.end()) {
        if (itr->value().is_object()) {
            auto const& group = itr->value().as_object();
            std::string const currentContext = std::string{ context } + std::string{ GetFieldName() };
            if (auto const result = m_retryOptions.Merge(group, currentContext, optGlobalOptions); result.first != StatusCode::Success) {
                return result;
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMismatchedValueTypeMessage("object", context, GetFieldName(), m_retryOptions.GetFieldName())
            };
        }
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::Connection::Write(
    boost::json::object& json, GlobalConnectionOptionsReference optGlobalOptions) const
{
    boost::json::object group;

    bool const shouldWriteTimeout =
        // Write if the timeout is not equal to the default connection timeout.
        !m_optTimeout.WouldMatchDefault(Defaults::ConnectionTimeout) && 
        // And, there is no global options, or if there are, the timeout is not equal to the global timeout option.
        (!optGlobalOptions || !m_optTimeout.WouldMatchDefault(optGlobalOptions->get().GetTimeout()));

    if (shouldWriteTimeout) {
        group[m_optTimeout.GetFieldName()] = m_optTimeout.GetSerializedValue();
    }

    auto const result = m_retryOptions.Write(group, optGlobalOptions);
    if (result.first != StatusCode::Success) {
        return result;
    }

    if (!group.empty()) { json.emplace(GetFieldName(), std::move(group)); }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::Connection::AreOptionsAllowable() const
{
    return { StatusCode::Success, "" }; // There are no requirements for this set of options. 
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Connection::GetTimeout() const
{
    return m_optTimeout.GetValueOrElse(Defaults::ConnectionTimeout);
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::int32_t Configuration::Options::Connection::GetRetryLimit() const
{
    return m_retryOptions.GetLimit();
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::chrono::milliseconds const& Configuration::Options::Connection::GetRetryInterval() const
{
    return m_retryOptions.GetInterval();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::SetTimeout(std::chrono::milliseconds const& value, bool& changed)
{
    if (!m_optTimeout.SetValue(value)) { return false; }
    changed = m_optTimeout.Modified();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Connection::SetRetryLimit(std::int32_t value, bool& changed)
{
    return m_retryOptions.SetLimit(value, changed);
}

//----------------------------------------------------------------------------------------------------------------------
    
bool Configuration::Options::Connection::SetRetryInterval(std::chrono::milliseconds const& value, bool& changed)
{
    return m_retryOptions.SetInterval(value, changed);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint()
    : m_transient()
    , m_protocol(
        local::StringToNetworkProtocol,
        local::StringFromNetworkProtocol,
        [] (auto const& value) { return value != ::Network::Protocol::Invalid; })
    , m_interface()
    , m_binding(
        [&] (auto const& value) {
            return ::Network::BindingAddress{ m_protocol.GetValue(), value, m_interface.GetValue() };
        },
        local::StringFromBindingAddress,
        [] (auto const& value) { return value.IsValid(); })
    , m_optBootstrap(
        [&] (auto const& value) {
            return ::Network::RemoteAddress{ m_protocol.GetValue(), value, true, ::Network::RemoteAddress::Origin::User };
        },
        local::StringFromRemoteAddress,
        [] (auto const& value) { return value.IsValid(); })
    , m_connectionOptions()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    ::Network::Protocol protocol,
    std::string_view interface,
    std::string_view binding,
    std::optional<std::string> const& optBootstrap)
    : m_transient({ .useBootstraps = true })
    , m_protocol(
        protocol,
        local::StringToNetworkProtocol,
        local::StringFromNetworkProtocol,
        [] (auto const& value) { return value != ::Network::Protocol::Invalid; })
    , m_interface(interface)
    , m_binding(
        binding,
        [&] (std::string_view value) {
            return ::Network::BindingAddress{ m_protocol.GetValue(), value, m_interface.GetValue() };
        },
        local::StringFromBindingAddress,
        [] (auto const& value) { return value.IsValid(); })
    , m_optBootstrap(
        optBootstrap,
        [&] (std::string_view value) {
            return ::Network::RemoteAddress{ m_protocol.GetValue(), value, true, ::Network::RemoteAddress::Origin::User };
        },
        local::StringFromRemoteAddress,
        [] (auto const& value) { return value.IsValid(); })
    , m_connectionOptions()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Endpoint::Endpoint(
    std::string_view protocol,
    std::string_view interface,
    std::string_view binding,
    std::optional<std::string> const& optBootstrap)
    : m_transient({ .useBootstraps = true })
    , m_protocol(
        protocol,
        local::StringToNetworkProtocol,
        local::StringFromNetworkProtocol,
        [] (auto const& value) { return value != ::Network::Protocol::Invalid; })
    , m_interface(interface)
    , m_binding(
        binding,
        [&] (std::string_view value) {
            return ::Network::BindingAddress{ m_protocol.GetValue(), value, m_interface.GetValue() };
        },
        local::StringFromBindingAddress,
        [] (auto const& value) { return value.IsValid(); })
    , m_optBootstrap(
        optBootstrap,
        [&] (std::string_view value) {
            return ::Network::RemoteAddress{ m_protocol.GetValue(), value, true, ::Network::RemoteAddress::Origin::User };
        },
        local::StringFromRemoteAddress,
        [] (auto const& value) { return value.IsValid(); })
    , m_connectionOptions()
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Endpoint::operator<=>(Endpoint const& other) const noexcept
{
    if (auto const result = m_protocol <=> other.m_protocol; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_interface <=> other.m_interface; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_binding <=> other.m_binding; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optBootstrap <=> other.m_optBootstrap; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_connectionOptions <=> other.m_connectionOptions; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::operator==(Endpoint const& other) const noexcept
{
    return m_protocol == other.m_protocol && 
           m_interface == other.m_interface && 
           m_binding == other.m_binding &&
           m_optBootstrap == other.m_optBootstrap &&
           m_connectionOptions == other.m_connectionOptions;
}
//
//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Endpoint::Merge(Endpoint& other)
{
    // The existing values of this object is chosen over the other's values. 
    if (m_protocol.NotModified()) { m_protocol = std::move(other.m_protocol); }
    if (m_interface.NotModified()) { m_interface = std::move(other.m_interface); }
    if (m_binding.NotModified()) { m_binding = std::move(other.m_binding); }
    if (m_optBootstrap.NotModified()) { m_optBootstrap = std::move(other.m_optBootstrap); }

    if (auto const result = m_connectionOptions.Merge(other.m_connectionOptions); result.first != StatusCode::Success) {
        return result;
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Endpoint::Merge(
    boost::json::object const& json,
    Runtime const& runtime,
    std::string_view context,
    GlobalConnectionOptionsReference optGlobalOptions)
{
    // JSON Schema:
    // "endpoints": [{
    //     "protocol": String,
    //     "interface": String,
    //     "binding": String,
    //     "bootstrap": Optional String,
    //     "connection": Optional Object,
    // }],

    // Capture the use bootstraps flag.
    m_transient.useBootstraps = runtime.useBootstraps;

    // Deserialize the protocol field from the endpoint object.
    if (m_protocol.NotModified()) {
        if (auto const itr = json.find(m_protocol.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_protocol.SetValueFromConfig(itr->value().as_string())) {
                    return {
                        StatusCode::InputError,
                        CreateUnexpectedValueMessage(allowable::ProtocolValues, context, m_protocol.GetFieldName())
                    };
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", context, m_protocol.GetFieldName())
                };
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMissingFieldMessage(context, m_protocol.GetFieldName())
            };
        }
    }

    // Deserialize the interface field from the endpoint object.
    if (m_interface.NotModified()) {
        if (auto const itr = json.find(m_interface.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_interface.SetValueFromConfig(itr->value().as_string())) {
                    return {
                        StatusCode::InputError,
                        CreateInvalidValueMessage(context, m_interface.GetFieldName())
                    };
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", context, m_interface.GetFieldName())
                };
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMissingFieldMessage(context, m_interface.GetFieldName())
            };
        }
    }

    // Deserialize the binding field from the endpoint object.
    if (m_binding.NotModified()) {
        if (auto const itr = json.find(m_binding.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_binding.SetValueFromConfig(itr->value().as_string())) {
                    return {
                        StatusCode::InputError,
                        CreateInvalidValueMessage(context, m_binding.GetFieldName())
                    };
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", context, m_binding.GetFieldName())
                };
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMissingFieldMessage(context, m_binding.GetFieldName())
            };
        }
    }

    // Deserialize the bootstrap field from the endpoint object.
    if (m_optBootstrap.NotModified()) {
        if (auto const itr = json.find(m_optBootstrap.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_optBootstrap.SetValueFromConfig(itr->value().as_string())) {
                    return {
                        StatusCode::InputError,
                        CreateInvalidValueMessage(context, m_optBootstrap.GetFieldName())
                    };
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", context, m_optBootstrap.GetFieldName())
                };
            }
        }
    }

    // Deserialize the retry object from the connection object.
    if (auto const itr = json.find(m_connectionOptions.GetFieldName()); itr != json.end()) {
        if (itr->value().is_object()) {
            auto const& group = itr->value().as_object();
            if (auto const result = m_connectionOptions.Merge(group, context, optGlobalOptions); result.first != StatusCode::Success) {
                return result;
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMismatchedValueTypeMessage("object", context, m_connectionOptions.GetFieldName())
            };
        }
    } else if (optGlobalOptions) {
        m_connectionOptions = *optGlobalOptions;
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::Endpoint::Write(
    boost::json::object& json, GlobalConnectionOptionsReference optGlobalOptions) const
{
    json[m_protocol.GetFieldName()] = m_protocol.GetSerializedValue();
    json[m_interface.GetFieldName()] = m_interface.GetValue();
    json[m_binding.GetFieldName()] = m_binding.GetSerializedValue();
    if (m_optBootstrap.HasValue()) { json[m_optBootstrap.GetFieldName()] = m_optBootstrap.GetSerializedValue(); }

    auto const result = m_connectionOptions.Write(json, optGlobalOptions);
    if (result.first != StatusCode::Success) {
        return result;
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::Endpoint::AreOptionsAllowable(std::string_view context) const
{
    if (m_protocol.GetValue() == ::Network::Protocol::Invalid) {
        return {
            StatusCode::InputError,
            CreateUnexpectedValueMessage(allowable::ProtocolValues, context, m_protocol.GetFieldName())
        };
    }

    if (m_interface.GetValue().empty()) {
        return {
            StatusCode::InputError,
            CreateInvalidValueMessage(context, m_interface.GetFieldName())
        };
    }

    if (!m_binding.GetValue().IsValid()) {
        return {
            StatusCode::InputError,
            CreateInvalidValueMessage(context, m_binding.GetFieldName())
        };
    }

    if (m_optBootstrap.HasValue() && !m_optBootstrap.GetValue().IsValid()) {
        return {
            StatusCode::InputError,
            CreateInvalidValueMessage(context, m_optBootstrap.GetFieldName())
        };
    }

    return m_connectionOptions.AreOptionsAllowable();
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Configuration::Options::Endpoint::GetProtocol() const { return m_protocol.GetValue(); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetProtocolString() const { return m_protocol.GetSerializedValue(); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetInterface() const { return m_interface.GetValue(); }

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress const& Configuration::Options::Endpoint::GetBinding() const { return m_binding.GetValue(); }

//----------------------------------------------------------------------------------------------------------------------

std::string const& Configuration::Options::Endpoint::GetBindingString() const { return m_binding.GetSerializedValue(); }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Network::RemoteAddress> const& Configuration::Options::Endpoint::GetBootstrap() const
{
    return m_optBootstrap.GetInternalOptionalValueMember();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::UseBootstraps() const { return m_transient.useBootstraps; }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Endpoint::GetConnectionTimeout() const
{
    return m_connectionOptions.GetTimeout();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Options::Endpoint::GetConnectionRetryLimit() const
{ 
    return m_connectionOptions.GetRetryLimit();
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Endpoint::GetConnectionRetryInterval() const
{
    return m_connectionOptions.GetRetryInterval();
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Endpoint::SetRuntimeOptions(Runtime const& runtime)
{
    m_transient.useBootstraps = runtime.useBootstraps;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionTimeout(std::chrono::milliseconds const& timeout, bool& changed)
{
    return m_connectionOptions.SetTimeout(timeout, changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionRetryLimit(std::int32_t limit, bool& changed)
{
    return m_connectionOptions.SetRetryLimit(limit, changed);
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Endpoint::SetConnectionRetryInterval(std::chrono::milliseconds const& interval, bool& changed)
{
    return m_connectionOptions.SetRetryInterval(interval, changed);
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Configuration::Options::Endpoint Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(
    ::Network::BindingAddress const& binding)
{
    Endpoint options{
        ::Network::Protocol::Test,
        binding.GetInterface(),
        binding.GetAuthority()
    };

    options.m_transient.useBootstraps = true;

    return options;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Configuration::Options::Endpoint Configuration::Options::Endpoint::CreateTestOptions<InvokeContext::Test>(
    std::string_view interface, std::string_view binding)
{
    Endpoint options{
        ::Network::Protocol::Test,
         interface,
         binding,
    };

    options.m_transient.useBootstraps = true;
    
    return options;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::Network()
    : m_endpoints()
    , m_connectionOptions()
    , m_optToken()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::operator==(Network const& other) const noexcept
{
    return m_endpoints == other.m_endpoints && 
           m_connectionOptions == other.m_connectionOptions && 
           m_optToken == other.m_optToken;
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Network::operator<=>(Network const& other) const noexcept
{
    if (auto const result = m_endpoints <=> other.m_endpoints; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_connectionOptions <=> other.m_connectionOptions; result != std::strong_ordering::equal) { return result; }
    if (auto const result = m_optToken <=> other.m_optToken; result != std::strong_ordering::equal) { return result; }
    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Network::Merge(
    boost::json::object const& json, Runtime const& runtime)
{
    // JSON Schema:
    // "network": {
    //   "endpoints": Array,
    //   "connection": Optional Object,
    //   "token": Optional String,
    // },

    // Deserialize the connection options object from the network object.
    if (auto const itr = json.find(m_connectionOptions.GetFieldName()); itr != json.end()) {
        if (itr->value().is_object()) {
            auto const& group = itr->value().as_object();
            if (auto const result = m_connectionOptions.Merge(group, GetFieldName()); result.first != StatusCode::Success) {
                return result;
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMismatchedValueTypeMessage("object", GetFieldName(), m_connectionOptions.GetFieldName())
            };
        }
    }
     
    // Deserialize the endpoints array from the network object.
    if (auto const itr = json.find(Symbols::Endpoints::GetFieldName()); itr != json.end()) {
        if (itr->value().is_array()) {
            for (auto const& [idx, value] : enumerate(itr->value().as_array())) {
                if (value.is_object()) {
                    Endpoint options;
                    
                    auto const result = options.Merge(
                        value.as_object(),
                        runtime,
                        CreateArrayContextString(idx, GetFieldName(), Symbols::Endpoints::GetFieldName()),
                        m_connectionOptions);

                    auto const itr = std::ranges::find_if(m_endpoints, [&options] (Endpoint const& existing) {
                        return options.GetBinding() == existing.GetBinding();
                    });
                    
                    if (itr == m_endpoints.end()) { 
                        m_endpoints.emplace_back(options);
                    } else { 
                       if (auto const result = itr->Merge(options); result.first != StatusCode::Success) {
                            return result;
                       }
                    }
                } else {
                    return { 
                        StatusCode::DecodeError,
                        CreateMismatchedValueTypeMessage("object", GetFieldName(), m_connectionOptions.GetFieldName())
                    };
                }
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMismatchedValueTypeMessage("array", GetFieldName(), Symbols::Endpoints::GetFieldName())
            };
        }
    }

    // If we were unable to deserialize any endpoints and we are running as the main process indicate, a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_endpoints.empty()) { 
        return { 
            StatusCode::InputError,
            CreateEmptyArrayFieldMessage(GetFieldName(), Symbols::Endpoints::GetFieldName())
        };
    }

    // Deserialize the token string from the network object.
    if (m_optToken.NotModified()) {
        if (auto const itr = json.find(m_optToken.GetFieldName()); itr != json.end()) {
            if (itr->value().is_string()) {
                if (!m_optToken.SetValueFromConfig(itr->value().as_string())) {
                    return {
                        StatusCode::InputError,
                        CreateInvalidValueMessage(GetFieldName(), m_optToken.GetFieldName())
                    };
                }
            } else {
                return { 
                    StatusCode::DecodeError,
                    CreateMismatchedValueTypeMessage("string", GetFieldName(), m_optToken.GetFieldName())
                };
            }
        }
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::Network::Write(boost::json::object& json) const
{
    boost::json::object group;

    // Serialize the endpoints into the json object.
    if (!m_endpoints.empty()) {
        boost::json::array endpoints;
        
        for (auto const& endpoint : m_endpoints) {
            boost::json::object object;
            if (auto const result = endpoint.Write(object, m_connectionOptions); result.first != StatusCode::Success) {
                return result;
            }
            endpoints.emplace_back(std::move(object));
        }

        group.emplace(Symbols::Endpoints::GetFieldName(), std::move(endpoints));
    }

    // Serialize the global connection options into the json object.
    if (auto const result = m_connectionOptions.Write(group); result.first != StatusCode::Success) {
        return result;
    }

    // Serialize the token string into the json object.
    if (m_optToken.HasValue()) { group[m_optToken.GetFieldName()] = m_optToken.GetValue(); }

    json.emplace(GetFieldName(), std::move(group));

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::Network::AreOptionsAllowable(Runtime const& runtime) const
{
    if (auto const status = m_connectionOptions.AreOptionsAllowable(); status.first != StatusCode::Success) {
        return status;
    }

    // If we were unable to deserialize any endpoints and we are running as the main process indicate, a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_endpoints.empty()) { 
        return { 
            StatusCode::InputError,
            CreateEmptyArrayFieldMessage(GetFieldName(), Symbols::Endpoints::GetFieldName())
        };
    }

    for (auto const& [idx, endpoint] : enumerate(m_endpoints)) {
        auto const status = endpoint.AreOptionsAllowable(
            CreateArrayContextString(idx, GetFieldName(), Symbols::Endpoints::GetFieldName()));
        if (status.first != StatusCode::Success) {
            return status;
        }
    }

    return { StatusCode::Success, "" };
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
        return protocol == existing.GetProtocol() && binding == existing.GetBindingString();
    });

    if (itr == m_endpoints.end()) { return {}; }
    return *itr;
}

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Network::GetConnectionTimeout() const
{
    return m_connectionOptions.GetTimeout();
}

//----------------------------------------------------------------------------------------------------------------------

std::int32_t Configuration::Options::Network::GetConnectionRetryLimit() const { return m_connectionOptions.GetRetryLimit(); }

//----------------------------------------------------------------------------------------------------------------------

std::chrono::milliseconds const& Configuration::Options::Network::GetConnectionRetryInterval() const
{
    return m_connectionOptions.GetRetryInterval();
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> const& Configuration::Options::Network::GetToken() const { return m_optToken.GetInternalOptionalValueMember(); }

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Network::FetchedEndpoint Configuration::Options::Network::UpsertEndpoint(
    Endpoint&& options, bool& changed)
{
    // Note: Updates to initializable fields must ensure the option set are always initialized in the store. 
    changed = true;
    auto const itr = std::ranges::find_if(m_endpoints, [&options] (Options::Endpoint const& existing) {
        return options.GetBindingString() == existing.GetBindingString();
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
        bool const found = protocol == existing.GetProtocol() && binding == existing.GetBindingString();
        if (found) { optExtractedOptions = std::move(existing); }
        return found;
    });

    changed = count == 1;
    return optExtractedOptions;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::SetConnectionTimeout(std::chrono::milliseconds const& timeout, bool& changed)
{
    if (!m_connectionOptions.SetTimeout(timeout, changed)) {
        return false;
    }

    bool const result = std::ranges::all_of(m_endpoints, [&timeout, &changed] (Endpoint& options) {
        return options.SetConnectionTimeout(timeout, changed);
    });

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::SetConnectionRetryLimit(std::int32_t limit, bool& changed)
{
    if (!m_connectionOptions.SetRetryLimit(limit, changed)) {
        return false;
    }

    bool const result = std::ranges::all_of(m_endpoints, [&limit, &changed] (Endpoint& options) {
        return options.SetConnectionRetryLimit(limit, changed);
    });

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

bool  Configuration::Options::Network::SetConnectionRetryInterval(std::chrono::milliseconds const& interval, bool& changed)
{
    if (!m_connectionOptions.SetRetryInterval(interval, changed)) {
        return false;
    }

    bool const result = std::ranges::all_of(m_endpoints, [&interval, &changed] (Endpoint& options) {
        return options.SetConnectionRetryInterval(interval, changed);
    });

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Network::SetToken(std::string_view const& token, bool& changed)
{
    if (!m_optToken.SetValue(token)) { return false; }
    changed = m_optToken.Modified();
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Algorithms::Algorithms(std::string_view fieldName)
    : m_fieldName(fieldName)
    , m_modified(false)
    , m_keyAgreements()
    , m_ciphers()
    , m_hashFunctions()
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::Algorithms::Algorithms(
    std::string_view fieldName,
    std::vector<std::string>&& keyAgreements,
    std::vector<std::string>&& ciphers,
    std::vector<std::string>&& hashFunctions)
    : m_fieldName(fieldName)
    , m_modified(false)
    , m_keyAgreements(std::move(keyAgreements))
    , m_ciphers(std::move(ciphers))
    , m_hashFunctions(std::move(hashFunctions))
{
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Algorithms::Merge(
    boost::json::object const& json, Runtime const& runtime)
{
    // Deserialize the key agreements from the cipher suite object.
    if (auto const itr = json.find(Symbols::KeyAgreements::GetFieldName()); itr != json.end()) {
        if (itr->value().is_array()) {
            auto const& elements = itr->value().as_array();
            if (elements.size() > ::Security::MaximumSupportedAlgorithmElements) {
                return {
                    StatusCode::InputError,
                    CreateExceededElementLinitMessage(
                        ::Security::MaximumSupportedAlgorithmElements,
                        Security::GetFieldName(), GetFieldName(), Symbols::KeyAgreements::GetFieldName())
                };
            }

            for (auto const& [idx, value] : enumerate(elements)) {
                if (value.is_string()) {
                    std::string_view keyAgreement = value.as_string();
                    if (keyAgreement.size() > ::Security::MaximumSupportedAlgorithmNameSize) {
                        return {
                            StatusCode::InputError,
                            CreateExceededCharacterLimitMessage(
                                ::Security::MaximumSupportedAlgorithmNameSize,
                                CreateArrayContextString(
                                    idx, Security::GetFieldName(), GetFieldName(), Symbols::KeyAgreements::GetFieldName()))
                        };
                    }

                    if (::Security::SupportedKeyAgreementNames.contains(keyAgreement.data())) {
                        m_keyAgreements.emplace_back(keyAgreement);
                    } else {
                        return {
                            StatusCode::InputError,
                            CreateInvalidValueMessage(
                                CreateArrayContextString(
                                    idx, Security::GetFieldName(), GetFieldName(), Symbols::KeyAgreements::GetFieldName()))
                        };
                    }
                } else {
                    return { 
                        StatusCode::DecodeError,
                        CreateMismatchedValueTypeMessage(
                            "string",
                            CreateArrayContextString(
                                idx, Security::GetFieldName(), GetFieldName(), Symbols::KeyAgreements::GetFieldName()))
                    };
                }
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMismatchedValueTypeMessage(
                    "array", Security::GetFieldName(), GetFieldName(), Symbols::KeyAgreements::GetFieldName())
            };
        }
    }

    // If we were unable to deserialize any key agreements and we are running as the main process, indicate a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_keyAgreements.empty()) {
        return {
            StatusCode::InputError,
            CreateEmptyArrayFieldMessage(
                Security::GetFieldName(), GetFieldName(), Symbols::KeyAgreements::GetFieldName())
        };
    }

    // Deserialize the ciphers from the cipher suite object.
    if (auto const itr = json.find(Symbols::Ciphers::GetFieldName()); itr != json.end()) {
        if (itr->value().is_array()) {
            auto const& elements = itr->value().as_array();
            if (elements.size() > ::Security::MaximumSupportedAlgorithmElements) {
                return {
                    StatusCode::InputError,
                    CreateExceededElementLinitMessage(
                        ::Security::MaximumSupportedAlgorithmElements,
                        Security::GetFieldName(), GetFieldName(), Symbols::Ciphers::GetFieldName())
                };
            }

            for (auto const& [idx, value] : enumerate(elements)) {
                if (value.is_string()) {
                    std::string_view ciphers = value.as_string();
                    if (ciphers.size() > ::Security::MaximumSupportedAlgorithmNameSize) {
                        return {
                            StatusCode::InputError,
                            CreateExceededCharacterLimitMessage(
                                ::Security::MaximumSupportedAlgorithmNameSize,
                                CreateArrayContextString(
                                    idx, Security::GetFieldName(), GetFieldName(), Symbols::Ciphers::GetFieldName()))
                        };
                    }

                    if (::Security::SupportedCipherNames.contains(ciphers.data())) {
                        m_ciphers.emplace_back(ciphers);
                    } else {
                        return {
                            StatusCode::InputError,
                            CreateInvalidValueMessage(
                                CreateArrayContextString(
                                    idx, Security::GetFieldName(), GetFieldName(), Symbols::Ciphers::GetFieldName()))
                        };
                    }
                } else {
                    return { 
                        StatusCode::DecodeError,
                        CreateMismatchedValueTypeMessage(
                            "string",
                            CreateArrayContextString(
                                idx, Security::GetFieldName(), GetFieldName(), Symbols::Ciphers::GetFieldName()))
                    };
                }
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMismatchedValueTypeMessage(
                    "array", Security::GetFieldName(), GetFieldName(), Symbols::Ciphers::GetFieldName())
            };
        }
    }

    // If we were unable to deserialize any ciphers and we are running as the main process, indicate a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_ciphers.empty()) {
        return {
            StatusCode::InputError,
            CreateEmptyArrayFieldMessage(
                Security::GetFieldName(), GetFieldName(), Symbols::Ciphers::GetFieldName())
        };
    }

    // Deserialize the hash functions from the cipher suite object.
    if (auto const itr = json.find(Symbols::HashFunctions::GetFieldName()); itr != json.end()) {
        if (itr->value().is_array()) {
            auto const& elements = itr->value().as_array();
            if (elements.size() > ::Security::MaximumSupportedAlgorithmElements) {
                return {
                    StatusCode::InputError,
                    CreateExceededElementLinitMessage(
                        ::Security::MaximumSupportedAlgorithmElements,
                        Security::GetFieldName(), GetFieldName(), Symbols::HashFunctions::GetFieldName())
                };
            }

            for (auto const& [idx, value] : enumerate(elements)) {
                if (value.is_string()) {
                    std::string_view hashFunction = value.as_string();
                    if (hashFunction.size() > ::Security::MaximumSupportedAlgorithmNameSize) {
                        return {
                            StatusCode::InputError,
                            CreateExceededCharacterLimitMessage(
                                ::Security::MaximumSupportedAlgorithmNameSize,
                                CreateArrayContextString(
                                    idx, Security::GetFieldName(), GetFieldName(), Symbols::HashFunctions::GetFieldName()))
                        };
                    }

                    if (::Security::SupportedHashFunctionNames.contains(hashFunction.data())) {
                        m_hashFunctions.emplace_back(hashFunction);
                    } else {
                        return {
                            StatusCode::InputError,
                            CreateInvalidValueMessage(
                                CreateArrayContextString(
                                    idx, Security::GetFieldName(), GetFieldName(), Symbols::HashFunctions::GetFieldName()))
                        };
                    }
                } else {
                    return { 
                        StatusCode::DecodeError,
                        CreateMismatchedValueTypeMessage(
                            "string",
                            CreateArrayContextString(
                                idx, Security::GetFieldName(), GetFieldName(), Symbols::HashFunctions::GetFieldName()))
                    };
                }
            }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMismatchedValueTypeMessage(
                    "array", Security::GetFieldName(), GetFieldName(), Symbols::HashFunctions::GetFieldName())
            };
        }
    }

    // If we were unable to deserialize any hash functions and we are running as the main process, indicate a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_hashFunctions.empty()) {
        return {
            StatusCode::InputError,
            CreateEmptyArrayFieldMessage(
                Security::GetFieldName(), GetFieldName(), Symbols::HashFunctions::GetFieldName())
        };
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::Algorithms::Write(boost::json::object& json) const
{
    boost::json::object object;

    {
        boost::json::array keyAgreements;
        keyAgreements.reserve(m_keyAgreements.size());
        std::ranges::transform(m_keyAgreements, std::back_inserter(keyAgreements), [](auto const& item) {
            return boost::json::string{ item };
        });
        object.emplace(Symbols::KeyAgreements::GetFieldName(), std::move(keyAgreements));
    }

    {
        boost::json::array ciphers;
        ciphers.reserve(m_ciphers.size());
        std::ranges::transform(m_ciphers, std::back_inserter(ciphers), [](auto const& item) {
            return boost::json::string{ item };
        });
        object.emplace(Symbols::Ciphers::GetFieldName(), std::move(ciphers));
    }

    {
        boost::json::array hashFunctions;
        hashFunctions.reserve(m_hashFunctions.size());
        std::ranges::transform(m_hashFunctions, std::back_inserter(hashFunctions), [](auto const& item) {
            return boost::json::string{ item };
        });
        object.emplace(Symbols::HashFunctions::GetFieldName(), std::move(hashFunctions));
    }

    json.emplace(GetFieldName(), std::move(object));

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::Algorithms::AreOptionsAllowable(Runtime const& runtime) const
{
    // If we were unable to deserialize any key agreements and we are running as the main process, indicate a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_keyAgreements.empty()) {
        return {
            StatusCode::InputError,
            CreateEmptyArrayFieldMessage(
                Security::GetFieldName(), GetFieldName(), Symbols::KeyAgreements::GetFieldName())
        };
    }

    // If we were unable to deserialize any ciphers and we are running as the main process, indicate a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_ciphers.empty()) {
        return {
            StatusCode::InputError,
            CreateEmptyArrayFieldMessage(
                Security::GetFieldName(), GetFieldName(), Symbols::Ciphers::GetFieldName())
        };
    }

    // If we were unable to deserialize any hash functions and we are running as the main process, indicate a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_hashFunctions.empty()) {
        return {
            StatusCode::InputError,
            CreateEmptyArrayFieldMessage(
                Security::GetFieldName(), GetFieldName(), Symbols::HashFunctions::GetFieldName())
        };
    }

    if (!HasAtLeastOneAlgorithmForEachComponent()) {
        return {
            StatusCode::InputError,
            "The security." + GetFieldName() + " field must contain at least one element in each algorithm array."
        };
    }

    return { StatusCode::Success, "" }; // There are no requirements for this set of options. 
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Algorithms::HasAtLeastOneAlgorithmForEachComponent() const
{
    return !m_keyAgreements.empty() && !m_ciphers.empty() && !m_hashFunctions.empty();
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Algorithms::SetKeyAgreements(std::vector<std::string>&& keyAgreements, bool& changed)
{
    if (std::ranges::equal(m_keyAgreements, keyAgreements)) { return true; }

    auto const validated = std::ranges::all_of(keyAgreements, [] (std::string const& value) {
        return ::Security::SupportedKeyAgreementNames.contains(value);
    });
    if (!validated) { return false; }
    
    m_keyAgreements = std::move(keyAgreements);
    m_modified = true;
    changed = true;
    
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Algorithms::SetCiphers(std::vector<std::string>&& ciphers, bool& changed)
{
    if (std::ranges::equal(m_ciphers, ciphers)) { return true; }
    
    auto const validated = std::ranges::all_of(ciphers, [] (std::string const& value) {
        return ::Security::SupportedCipherNames.contains(value);
    });
    if (!validated) { return false; }

    m_ciphers = std::move(ciphers);
    m_modified = true;
    changed = true;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Algorithms::SetHashFunctions(std::vector<std::string>&& hashFunctions, bool& changed)
{
    if (std::ranges::equal(m_hashFunctions, hashFunctions)) { return true; }
    
    auto const validated = std::ranges::all_of(hashFunctions, [] (std::string const& value) {
        return ::Security::SupportedHashFunctionNames.contains(value);
    });
    if (!validated) { return false; }

    m_hashFunctions = std::move(hashFunctions);
    m_modified = true;
    changed = true;
    
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::SupportedAlgorithms::SupportedAlgorithms()
    : m_modified(false)
    , m_container()
{
};

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::SupportedAlgorithms::SupportedAlgorithms(Container&& container)
    : m_modified(false)
    , m_container(std::move(container))
{
};

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::SupportedAlgorithms::operator==(SupportedAlgorithms const& other) const noexcept
{
    if (m_container.size() != other.m_container.size()) {
        return false;
    }

    constexpr auto compare = [] (Algorithms const& lhs, Algorithms const& rhs) {
        return lhs.GetKeyAgreements() == rhs.GetKeyAgreements() &&
               lhs.GetCiphers() == rhs.GetCiphers() &&
               lhs.GetHashFunctions() == rhs.GetHashFunctions();
    };

    // Compare the Algorithms objects
    for (auto itr = m_container.begin(), jtr = other.m_container.begin(); itr != m_container.end(); ++itr, ++jtr) {
        if (!compare(itr->second, jtr->second)) { return false; }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::SupportedAlgorithms::operator<=>(SupportedAlgorithms const& other) const noexcept
{
    if (auto const result = m_container.size() <=> other.m_container.size(); result != std::strong_ordering::equal) {
        return result;
    }

    constexpr auto compare = [] (Algorithms const& lhs, Algorithms const& rhs) {
        if (auto const result = lhs.GetKeyAgreements() <=> rhs.GetKeyAgreements(); result != std::strong_ordering::equal) {
            return result;
        }

        if (auto const result = lhs.GetCiphers() <=> rhs.GetCiphers(); result != std::strong_ordering::equal) {
            return result;
        }

        if (auto const result = lhs.GetHashFunctions() <=> rhs.GetHashFunctions(); result != std::strong_ordering::equal) {
            return result;
        }

        return std::strong_ordering::equal;
    };

    // Compare the Algorithms objects
    for (auto itr = m_container.begin(), jtr = other.m_container.begin(); itr != m_container.end(); ++itr, ++jtr) {
        if (auto const result = compare(itr->second, jtr->second); result != std::strong_ordering::equal) {
            return result;
        }
    }

    return std::strong_ordering::equal;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::SupportedAlgorithms::Merge(
    boost::json::object const& json, Runtime const& runtime)
{
    if (m_modified) {
        return { StatusCode::Success, "" }; // Take the existing set of algorithms over the set contained in the file.  
    }

    if (json.size() > ::Security::SupportedConfidentialityLevelSize) {
        return {
            StatusCode::InputError,
            CreateExceededElementLinitMessage(
                ::Security::SupportedConfidentialityLevelSize, Security::GetFieldName(), GetFieldName())
        };
    }

    for (auto const& [key, value] : json) {
        auto const optLevel = local::StringToConfidentialityLevel(key);
        if (!optLevel) {
            return {
                StatusCode::InputError,
                CreateUnexpectedFieldMessage(
                    allowable::ConfidentialityValues, Security::GetFieldName(), GetFieldName(), key)
            };
        }

        Algorithms algorithms{ key };
        if (auto const result = algorithms.Merge(value.as_object(), runtime); result.first != StatusCode::Success) {
            return result;
        }

        m_container.emplace(*optLevel, std::move(algorithms));
    }

    // If we were unable to deserialize any algorithms and we are running as the main process, indicate a bad state would occur. 
    if (runtime.context == RuntimeContext::Foreground && m_container.empty()) {
        return {
            StatusCode::DecodeError,
            CreateMissingFieldMessage(Security::GetFieldName(), GetFieldName())
        };
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::SupportedAlgorithms::Write(
    boost::json::object& json) const
{
    auto const [itr, emplaced] = json.emplace(GetFieldName(), boost::json::object{});
    assert(emplaced);

    auto& group = itr->value().as_object();

    for (auto const& [level, algorithms] : m_container) {
        if (auto const result = algorithms.Write(group); result.first != StatusCode::Success) {
            return result;
        }
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::SupportedAlgorithms::AreOptionsAllowable(Runtime const& runtime) const
{
    if (runtime.context == RuntimeContext::Foreground && m_container.empty()) {
        return {
            StatusCode::InputError,
            CreateMissingFieldMessage(Security::GetFieldName(), GetFieldName())
        };
    }

    for (auto const& [level, algorithms] : m_container) {
        auto const status = algorithms.AreOptionsAllowable(runtime);
        if (status.first != StatusCode::Success) {
            return status;
        }
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::SupportedAlgorithms::HasAlgorithmsForLevel(::Security::ConfidentialityLevel level) const
{
    if (auto const itr = m_container.find(level); itr != m_container.end()) {
        return itr->second.HasAtLeastOneAlgorithmForEachComponent();
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::SupportedAlgorithms::FetchedAlgorithms Configuration::Options::SupportedAlgorithms::FetchAlgorithms(
    ::Security::ConfidentialityLevel level) const
{
    if (auto const itr = m_container.find(level); itr != m_container.end()) {
        return itr->second;
    }
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::SupportedAlgorithms::ForEachSupportedAlgorithm(AlgorithmsReader const& reader) const
{
    for (auto const& [level, algorithms] : m_container) {
        if (reader(level, algorithms) != CallbackIteration::Continue) { return false; }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::SupportedAlgorithms::ForEachSupportedKeyAgreement(KeyAgreementReader const& reader) const
{
    for (auto const& [level, algorithms] : m_container) {
        for (auto const& keyAgreement : algorithms.GetKeyAgreements()) {
            if (reader(level, keyAgreement) != CallbackIteration::Continue) { return false; }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::SupportedAlgorithms::ForEachSupportedCipher(CipherReader& reader) const
{
    for (auto const& [level, algorithms] : m_container) {
        for (auto const& cipher : algorithms.GetCiphers()) {
            if (reader(level, cipher) != CallbackIteration::Continue) { return false; }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::SupportedAlgorithms::ForEachSupportedHashFunction(HashFunctionReader& reader) const
{
    for (auto const& [level, algorithms] : m_container) {
        for (auto const& hashFunction : algorithms.GetHashFunctions()) {
            if (reader(level, hashFunction) != CallbackIteration::Continue) { return false; }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::SupportedAlgorithms::SetAlgorithmsAtLevel(
    ::Security::ConfidentialityLevel level,
    std::vector<std::string> keyAgreements,
    std::vector<std::string> ciphers,
    std::vector<std::string> hashFunctions,
    bool& changed)
{
    auto const optFieldName = local::StringFromConfidentialityLevel(level);
    if (!optFieldName) { return false; }

    auto const& [itr, emplaced] = m_container.emplace(level, *optFieldName);
    auto& [key, value] = *itr;

    // Could pass in on construction and verify none of the algorithm vectors are empty

    {
        bool const success = value.SetKeyAgreements(std::move(keyAgreements), changed);
        if (!success) {
            // If this is a new entry in the container, but validating the algorithms failed, remove the entry. 
            if (emplaced) { m_container.erase(itr); } 
            return false;
        }
    }

    {
        bool const success = value.SetCiphers(std::move(ciphers), changed);
        if (!success) {
            // If this is a new entry in the container, but validating the algorithms failed, remove the entry. 
            if (emplaced) { m_container.erase(itr); } 
            return false;
        }
    }
    {
        bool const success = value.SetHashFunctions(std::move(hashFunctions), changed);
        if (!success) {
            // If this is a new entry in the container, but validating the algorithms failed, remove the entry. 
            if (emplaced) { m_container.erase(itr); } 
            return false;
        }
    }

    m_modified = value.Modified();
    
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Security::operator==(Security const& other) const noexcept
{
    return m_supportedAlgorithms == other.m_supportedAlgorithms;
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Configuration::Options::Security::operator<=>(Security const& other) const noexcept
{
    return m_supportedAlgorithms <=> other.m_supportedAlgorithms;
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::DeserializationResult Configuration::Options::Security::Merge(
    boost::json::object const& json, Runtime const& runtime)
{
    // JSON Schema:
    // "security": {
    //     "algorithms": {
    //         "<level>" : {
    //             "key_agreements" : Array[String],
    //             "ciphers" : Array[String],
    //             "hash_functions" : Array[String],
    //         },
    //     }
    // }

    // Deserialize the cipher suites stored in the algorithms objects. 
    if (auto const itr = json.find(m_supportedAlgorithms.GetFieldName()); itr != json.end()) {
        if (itr->value().is_object()) {
        if (auto const result = m_supportedAlgorithms.Merge(itr->value().as_object(), runtime); result.first != StatusCode::Success) {
            return result;
        }
        } else {
            return { 
                StatusCode::DecodeError,
                CreateMismatchedValueTypeMessage("object", Security::GetFieldName(), GetFieldName())
            };
        }
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::SerializationResult Configuration::Options::Security::Write(boost::json::object& json) const
{
    auto const [itr, emplaced] = json.emplace(GetFieldName(), boost::json::object{});
    assert(emplaced);

    auto& group = itr->value().as_object();

    // Serialize the supported algorithms into the json object.
    if (auto const result = m_supportedAlgorithms.Write(group); result.first != StatusCode::Success) {
        return result;
    }

    if (group.empty()) {
        return { StatusCode::InputError, "" };
    }

    return { StatusCode::Success, "" };
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::ValidationResult Configuration::Options::Security::AreOptionsAllowable(Runtime const& runtime) const
{
    return m_supportedAlgorithms.AreOptionsAllowable(runtime);
}

//----------------------------------------------------------------------------------------------------------------------

Configuration::Options::SupportedAlgorithms const& Configuration::Options::Security::GetSupportedAlgorithms() const
{
    return m_supportedAlgorithms;
}

//----------------------------------------------------------------------------------------------------------------------

bool Configuration::Options::Security::SetSupportedAlgorithmsAtLevel(
    ::Security::ConfidentialityLevel level,
    std::vector<std::string> keyAgreements,
    std::vector<std::string> ciphers,
    std::vector<std::string> hashFunctions,
    bool& changed)
{
    return m_supportedAlgorithms.SetAlgorithmsAtLevel(
        level, std::move(keyAgreements), std::move(ciphers), std::move(hashFunctions), changed);
}

//----------------------------------------------------------------------------------------------------------------------

void Configuration::Options::Security::ClearSupportedAlgorithms() { m_supportedAlgorithms.Clear(); }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Configuration::Options::Identifier::Persistence> local::StringToPersistence(std::string_view value)
{
    return allowable::IfAllowableGetValue(allowable::PersistenceValues, value);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> local::StringFromPersistence(Options::Identifier::Persistence value)
{
    return allowable::IfAllowableGetString(allowable::PersistenceValues, value);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::SharedIdentifier> local::StringToSharedIdentifier(std::string_view value)
{
    auto const spIdentifier = (value.empty()) ? 
        std::make_shared<Node::Identifier const>(Node::GenerateIdentifier()) : 
        std::make_shared<Node::Identifier const>(value);
    return spIdentifier->IsValid() ? std::make_optional(spIdentifier) : std::nullopt;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> local::StringFromSharedIdentifier(Node::SharedIdentifier const& spIdentifier)
{
    return (spIdentifier) ? std::make_optional(spIdentifier->ToExternal()) : std::nullopt;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> local::StringFromMilliseconds(std::chrono::milliseconds const& value)
{
    assert(value <= std::chrono::hours{ 24 });

    using namespace std::chrono_literals;
    static std::map<std::chrono::milliseconds, std::string> const durations = { 
        { 0ms, "ms" }, // 0 - 999 milliseconds 
        { 1'000ms, "s" }, // 1 - 59 seconds
        { 60'000ms, "min" }, // 1 - ? minutes
        { std::chrono::milliseconds::max(), "?" } // Ensures we get a proper mapping for minutes. 
    };

    // Get the first value that is less than or equal to the provided value, start the search from the largest bound. 
    auto const bounds = durations | std::views::reverse | std::views::keys;
    auto const bound = std::ranges::find_if(bounds, [&] (auto const& duration) { return value >= duration;  });
    assert(bound.base() != durations.rbegin()); // The provided value should never greater than the millisecond max. 
    auto const divisor = std::max((*bound).count(), std::int64_t(1)); // Bind the zero bound to one. 
    auto const& postfix = bound.base()->second;
    return std::to_string(value.count() / divisor).append(postfix);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::chrono::milliseconds> local::StringToMilliseconds(std::string_view value)
{
    constexpr auto ConstructDuration = [] (std::string_view value) -> std::optional<std::chrono::milliseconds> {
        enum class Duration : std::uint32_t { Milliseconds, Seconds, Minutes, Hours, Days, Weeks, Invalid };

        constexpr auto ParseDuration = [] (std::string_view value) -> std::pair<Duration, std::size_t> {
            static std::unordered_map<Duration, std::string> const durations = { 
                { Duration::Milliseconds, "ms" },
                { Duration::Seconds, "s" },
                { Duration::Minutes, "min" },
                { Duration::Hours, "h" },
                { Duration::Days, "d" },
                { Duration::Weeks, "w" }
            };

            // Find the first instance of a non-numeric character. 
            auto const first = std::ranges::find_if(value, [] (char c) { return std::isalpha(c); });
            if (first == value.end()) {
                return { Duration::Invalid, 0 }; // No postfix was found. 
            }

            // Attempt to map the postfix to a duration entry. 
            std::string_view const postfix = { first, value.end() };
            auto const entry = std::ranges::find_if(durations | std::views::values, [&] (std::string_view const& value) { 
                return postfix == value;
            });

            // If no mapping could be found, indicate an error has occurred. 
            if (entry.base() == durations.end()) { return { Duration::Invalid, 0 }; }

            // Returns the type of entry and the number of characters in the numeric value. 
            return { entry.base()->first, value.size() - postfix.size() };
        };

        try {
            auto const [duration, size] = ParseDuration(value); // Get the duration and end of the numeric value.
            if (duration == Duration::Invalid || size == 0) { return {}; } // No valid duration was found.

            // Attempt to cast the provided value to a number and then adjust it to milliseconds. 
            auto count = boost::lexical_cast<std::uint32_t>(value.data(), size);
            switch (duration) {
                case Duration::Weeks: count *= 7; [[fallthrough]];
                case Duration::Days: count *= 24; [[fallthrough]];
                case Duration::Hours: count *= 60; [[fallthrough]];
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

    return ConstructDuration(value);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Network::Protocol> local::StringToNetworkProtocol(std::string_view value)
{
    return allowable::IfAllowableGetValue(allowable::ProtocolValues, value);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> local::StringFromNetworkProtocol(Network::Protocol const& value)
{
    return allowable::IfAllowableGetString(allowable::ProtocolValues, value);
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::optional<std::string> local::StringFromBindingAddress(Network::BindingAddress const& value)
{
    return std::string{ value.GetAuthority() };
}

//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::optional<std::string> local::StringFromRemoteAddress(Network::RemoteAddress const& value)
{
    return std::string{ value.GetAuthority() };
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Security::ConfidentialityLevel>  local::StringToConfidentialityLevel(std::string_view value)
{
    return allowable::IfAllowableGetValue(allowable::ConfidentialityValues, value);
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::string> local::StringFromConfidentialityLevel(Security::ConfidentialityLevel value)
{
    return allowable::IfAllowableGetString(allowable::ConfidentialityValues, value);
}

//----------------------------------------------------------------------------------------------------------------------
