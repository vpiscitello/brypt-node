//----------------------------------------------------------------------------------------------------------------------
// Description: C++20 language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/helpers.hpp>
#include <brypt/identifier.hpp>
#include <brypt/protocol.hpp>
#include <brypt/result.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <any>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

class option;
class algorithms_package;
class endpoint_options;

enum class confidentiality_level : std::int32_t {
    unknown = BRYPT_UNKNOWN,
    low = BRYPT_CONFIDENTIALITY_LEVEL_LOW,
    medium = BRYPT_CONFIDENTIALITY_LEVEL_MEDIUM,
    high = BRYPT_CONFIDENTIALITY_LEVEL_HIGH
};

enum class log_level : std::int32_t {
    unknown = BRYPT_UNKNOWN,
    off = BRYPT_LOG_LEVEL_OFF,
    trace = BRYPT_LOG_LEVEL_TRACE,
    debug = BRYPT_LOG_LEVEL_DEBUG,
    info = BRYPT_LOG_LEVEL_INFO,
    warn = BRYPT_LOG_LEVEL_WARNING,
    err = BRYPT_LOG_LEVEL_ERROR,
    critical = BRYPT_LOG_LEVEL_CRITICAL,
};

using supported_algorithms = std::map<confidentiality_level, algorithms_package>;

// Note: Supported primitive and string types are specified separately to allow the std::string_view constructor. 
// Otherwise, std::string would be taken by reference instead of copy and char const*/char const[] would not be allowed.
template<typename ValueType>
concept supported_primitive_type =
    std::is_same_v<std::remove_cvref_t<ValueType>, bool> ||
    std::is_same_v<std::remove_cvref_t<ValueType>, std::int32_t> ||
    std::is_same_v<std::underlying_type_t<std::remove_cvref_t<ValueType>>, std::int32_t> ||
    std::is_same_v<std::remove_cvref_t<ValueType>, std::chrono::milliseconds>;

template<typename ValueType>
concept supported_string_type = std::is_same_v<std::remove_cvref_t<ValueType>, std::string>;

template<typename ValueType>
concept supported_option_type = supported_primitive_type<ValueType> || supported_string_type<ValueType>;

template<supported_option_type ValueType>
[[nodiscard]] constexpr bool matches_option_type(brypt_option_t option);

//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::option {
//----------------------------------------------------------------------------------------------------------------------

class brypt::option
{
public:
    enum runtime_options : brypt_option_t {
        base_path = BRYPT_OPT_BASE_FILEPATH,
        configuration_filename = BRYPT_OPT_CONFIGURATION_FILENAME,
        bootstrap_filename = BRYPT_OPT_BOOTSTRAP_FILENAME,
        core_threads = BRYPT_OPT_CORE_THREADS,
        use_bootstraps = BRYPT_OPT_USE_BOOTSTRAPS,
        log_level = BRYPT_OPT_LOG_LEVEL
    };

    enum serialized_options : brypt_option_t {
        identifier_persistence = BRYPT_OPT_IDENTIFIER_PERSISTENCE,
        node_name = BRYPT_OPT_NODE_NAME,
        node_description = BRYPT_OPT_NODE_DESCRIPTION,
        connection_timeout = BRYPT_OPT_CONNECTION_TIMEOUT,
        connection_retry_limit = BRYPT_OPT_CONNECTION_RETRY_LIMIT,
        connection_retry_interval = BRYPT_OPT_CONNECTION_RETRY_INTERVAL
    };

    using base_path_t = std::string;
    using bootstrap_filename_t = std::string;
    using configuration_filename_t = std::string;
    using core_threads_t = std::int32_t;
    using identifier_persistence_t = brypt::identifier_persistence;
    using use_bootstraps_t = bool;
    using node_name_t = std::string;
    using node_description_t = std::string;
    using log_level_t = brypt::log_level;
    using connection_timeout_t = std::chrono::milliseconds;
    using connection_retry_limit_t = std::int32_t;
    using connection_retry_interval_t = std::chrono::milliseconds;

    option();
    option(brypt_option_t name, std::string_view value);
    option(brypt_option_t name, std::string_view value, result& result) noexcept;

    template<supported_primitive_type ValueType>
    option(brypt_option_t name, ValueType value);

    template<supported_primitive_type ValueType>
    option(brypt_option_t name, ValueType value, result& result) noexcept;

    template<typename Rep, typename Period>
    option(brypt_option_t name, std::chrono::duration<Rep, Period> const& value);

    template<typename Rep, typename Period>
    option(brypt_option_t name, std::chrono::duration<Rep, Period> const& value, result& result) noexcept;

    [[nodiscard]] brypt_option_t name() const noexcept;
    [[nodiscard]] auto const& type() const noexcept;
    [[nodiscard]] bool has_value() const noexcept;

    template<supported_option_type ValueType>
    [[nodiscard]] bool contains() const noexcept;

    template<supported_option_type ValueType>
    [[nodiscard]] ValueType const& value() const;

    template<supported_option_type ValueType>
    [[nodiscard]] ValueType&& extract();

    template<supported_option_type ValueType>
    [[nodiscard]] result get(ValueType& value) const;

private:
    void on_invalid_value(result& result);

    brypt_option_t m_name;
    std::any m_value;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::option::option()
    : m_name(0)
    , m_value()
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::option::option(brypt_option_t name, std::string_view value)
    : m_name(name)
    , m_value(std::string(value))
{
    if (!matches_option_type<std::string>(name)) {
        throw brypt::result{ result_code::invalid_argument };
    }
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::option::option(brypt_option_t name, std::string_view value, result& result) noexcept
    : m_name(name)
    , m_value(std::string(value))
{
    if (!matches_option_type<std::string>(name)) {
        on_invalid_value(result);
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_primitive_type ValueType>
inline brypt::option::option(brypt_option_t name, ValueType value)
    : m_name(name)
    , m_value(value)
{
    if (!matches_option_type<ValueType>(name)) {
        throw brypt::result{ result_code::invalid_argument };
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_primitive_type ValueType>
inline brypt::option::option(brypt_option_t name, ValueType value, result& result) noexcept
    : m_name(name)
    , m_value(value)
{
    if (!matches_option_type<ValueType>(name)) {
        on_invalid_value(result);
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<typename Rep, typename Period>
inline brypt::option::option(brypt_option_t name, std::chrono::duration<Rep, Period> const& value)
    : m_name(name)
    , m_value(value)
{
    switch (m_name) {
        case option::connection_timeout:
        case option::connection_retry_interval: break;
        default: throw brypt::result{ result_code::invalid_argument }; 
    }

    m_value = std::chrono::duration_cast<std::chrono::milliseconds>(value);
}

//----------------------------------------------------------------------------------------------------------------------

template<typename Rep, typename Period>
inline brypt::option::option(brypt_option_t name, std::chrono::duration<Rep, Period> const& value, result& result) noexcept
    : m_name(name)
    , m_value(value)
{
    switch (m_name) {
        case option::connection_timeout:
        case option::connection_retry_interval: break;
        default: on_invalid_value(result); return;
    }

    m_value = std::chrono::duration_cast<std::chrono::milliseconds>(value);
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt_option_t brypt::option::name() const noexcept { return m_name; }

//----------------------------------------------------------------------------------------------------------------------

inline auto const& brypt::option::type() const noexcept { return m_value.type(); }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::option::has_value() const noexcept { return m_value.has_value(); }

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_option_type ValueType>
inline bool brypt::option::contains() const noexcept { return m_value.type() == typeid(ValueType); }

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_option_type ValueType>
inline ValueType const& brypt::option::value() const
{
    try {
        return std::any_cast<ValueType const&>(m_value);
    }
    catch (...) { throw brypt::result{ result_code::invalid_argument }; }
}

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_option_type ValueType>
inline ValueType&& brypt::option::extract()
{
    try {
        return std::any_cast<ValueType&&>(std::move(m_value));
    }
    catch (...) { throw brypt::result{ result_code::invalid_argument }; }
}

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_option_type ValueType>
inline brypt::result brypt::option::get(ValueType& value) const
{
    try {
        value = std::any_cast<ValueType const&>(m_value);
        return result_code::accepted;
    }
    catch (...) { return result_code::invalid_argument; }
}

//----------------------------------------------------------------------------------------------------------------------

inline void brypt::option::on_invalid_value(result& result)
{
    m_value.reset();
    result = result_code::invalid_argument;
}

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_option_type ValueType>
constexpr bool brypt::matches_option_type(brypt_option_t name)
{
    switch (name) {
        case option::use_bootstraps: {
            return std::is_same_v<ValueType, bool>;
        }
        case option::core_threads:
        case option::connection_retry_limit: {
            return std::is_same_v<ValueType, std::int32_t>;
        }
        case option::identifier_persistence:
        case option::log_level: {
            if constexpr (std::is_enum_v<ValueType>) {
                return std::is_same_v<std::underlying_type_t<ValueType>, std::int32_t>;
            }
            else {
                return false;
            }
        }
        case option::connection_timeout:
        case option::connection_retry_interval: {
            return std::is_same_v<ValueType, std::int32_t> || std::is_same_v<ValueType, std::chrono::milliseconds>;
        }
        case option::base_path:
        case option::configuration_filename:
        case option::bootstrap_filename:
        case option::node_name:
        case option::node_description: {
            return std::is_same_v<ValueType, std::string>;
        }
        default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// } brypt::option
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::algorithms_package {
//----------------------------------------------------------------------------------------------------------------------

class brypt::algorithms_package
{
public:
    algorithms_package() noexcept = default;
    
    explicit algorithms_package(brypt_option_algorithms_package_t const* const options);

    algorithms_package(
        std::vector<std::string> key_agreements,
        std::vector<std::string> ciphers,
        std::vector<std::string> hash_functions);

    ~algorithms_package() = default;
    algorithms_package(algorithms_package const& other) = default;
    algorithms_package(algorithms_package&& other) = default;
    algorithms_package& operator=(algorithms_package const& other) = default;
    algorithms_package& operator=(algorithms_package&& other) = default;
    
    std::vector<std::string> const& get_key_agreements() const { return m_key_agreements; }
    std::vector<std::string> const& get_ciphers() const { return m_ciphers; }
    std::vector<std::string> const& get_hash_functions() const { return m_hash_functions; }

private:
    std::vector<std::string> m_key_agreements;
    std::vector<std::string> m_ciphers;
    std::vector<std::string> m_hash_functions;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::algorithms_package::algorithms_package(
    brypt_option_algorithms_package_t const* const options)
    : m_key_agreements()
    , m_ciphers()
    , m_hash_functions()
{
    if (options) {
        m_key_agreements.reserve(options->key_agreements_size);
        for (std::size_t idx = 0; idx < options->key_agreements_size; ++idx) {
            m_key_agreements.emplace_back(options->key_agreements[idx]);
        }

        m_ciphers.reserve(options->ciphers_size);
        for (std::size_t idx = 0; idx < options->ciphers_size; ++idx) {
            m_ciphers.emplace_back(options->ciphers[idx]);
        }

        m_hash_functions.reserve(options->hash_functions_size);
        for (std::size_t idx = 0; idx < options->hash_functions_size; ++idx) {
            m_hash_functions.emplace_back(options->hash_functions[idx]);
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::algorithms_package::algorithms_package(
    std::vector<std::string> key_agreements,
    std::vector<std::string> ciphers,
    std::vector<std::string> hash_functions)
    : m_key_agreements(std::move(key_agreements))
    , m_ciphers(std::move(ciphers))
    , m_hash_functions(std::move(hash_functions))
{
}

//----------------------------------------------------------------------------------------------------------------------
// } brypt::algorithms_package
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::endpoint_options {
//----------------------------------------------------------------------------------------------------------------------

class brypt::endpoint_options
{
public:
    endpoint_options() noexcept = default;

    explicit endpoint_options(brypt_option_endpoint_t const* const options);

    endpoint_options(
        brypt::protocol protocol,
        std::string_view interface,
        std::string_view binding,
        std::optional<std::string> const& bootstrap = {});

    ~endpoint_options() = default;
    endpoint_options(endpoint_options const& other) = default;
    endpoint_options(endpoint_options&& other) = default;
    endpoint_options& operator=(endpoint_options const& other) = default;
    endpoint_options& operator=(endpoint_options&& other) = default;

    operator brypt_option_endpoint_t() const noexcept;

    brypt::protocol get_protocol() const noexcept;
    std::string const& get_interface() const noexcept;
    std::string const& get_binding() const noexcept;
    std::optional<std::string> const& get_bootstrap() const noexcept;

    void set_protocol(brypt::protocol protocol);
    void set_interface(std::string_view interface);
    void set_binding(std::string_view binding);
    void set_bootstrap(std::optional<std::string> const& bootstrap);
    void reset_bootstrap();

private:
    brypt::protocol m_protocol;
    std::string m_interface;
    std::string m_binding;
    std::optional<std::string> m_bootstrap;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::endpoint_options::endpoint_options(brypt_option_endpoint_t const* const options)
    : m_protocol()
    , m_interface()
    , m_binding()
    , m_bootstrap()
{
    if (options) {
        m_protocol = static_cast<brypt::protocol>(options->protocol);
        m_interface = options->interface != nullptr ? options->interface : "";
        m_binding = options->binding != nullptr ? options->binding : "";
        if (options->bootstrap && strlen(options->bootstrap) != 0) {
            m_bootstrap = std::string{ options->bootstrap };
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::endpoint_options::endpoint_options(
    brypt::protocol protocol,
    std::string_view interface,
    std::string_view binding,
    std::optional<std::string> const& bootstrap)
    : m_protocol(protocol)
    , m_interface(interface)
    , m_binding(binding)
    , m_bootstrap(bootstrap)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::endpoint_options::operator brypt_option_endpoint_t() const noexcept
{
    return {
        .protocol = static_cast<std::int32_t>(m_protocol),
        .interface = m_interface.c_str(),
        .binding = m_binding.c_str(),
        .bootstrap = m_bootstrap ? m_bootstrap->c_str() : nullptr
    };
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::protocol brypt::endpoint_options::get_protocol() const noexcept { return m_protocol; }

//----------------------------------------------------------------------------------------------------------------------

inline std::string const& brypt::endpoint_options::get_interface() const noexcept { return m_interface; }

//----------------------------------------------------------------------------------------------------------------------

inline std::string const& brypt::endpoint_options::get_binding() const noexcept { return m_binding; }

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<std::string> const& brypt::endpoint_options::get_bootstrap() const noexcept { return m_bootstrap; }

//----------------------------------------------------------------------------------------------------------------------

inline void brypt::endpoint_options::set_protocol(brypt::protocol protocol) { m_protocol = protocol; }

//----------------------------------------------------------------------------------------------------------------------

inline void brypt::endpoint_options::set_interface(std::string_view interface) { m_interface = interface; }

//----------------------------------------------------------------------------------------------------------------------

inline void brypt::endpoint_options::set_binding(std::string_view binding) { m_binding = binding; }

//----------------------------------------------------------------------------------------------------------------------

inline void brypt::endpoint_options::set_bootstrap(std::optional<std::string> const& bootstrap)
{
    m_bootstrap = bootstrap;
}

//----------------------------------------------------------------------------------------------------------------------

inline void brypt::endpoint_options::reset_bootstrap() { m_bootstrap.reset(); }

//----------------------------------------------------------------------------------------------------------------------
// } brypt::endpoint_options
//----------------------------------------------------------------------------------------------------------------------
