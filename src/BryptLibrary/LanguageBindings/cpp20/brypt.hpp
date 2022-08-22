//----------------------------------------------------------------------------------------------------------------------
// Description: C++20 language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/action.hpp>
#include <brypt/helpers.hpp>
#include <brypt/identifier.hpp>
#include <brypt/options.hpp>
#include <brypt/peer.hpp>
#include <brypt/protocol.hpp>
#include <brypt/result.hpp>
#include <brypt/status.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <any>
#include <cassert>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <forward_list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

class service;

using message_callback = std::function<bool(std::string_view, std::span<std::uint8_t const>, next const&)>;
using log_callback = std::function<void(log_level, std::string_view)>;

enum class event : std::uint32_t {
    binding_failed,
    connection_failed,
    endpoint_started,
    endpoint_stopped,
    peer_connected,
    peer_disconnected,
    runtime_started,
    runtime_stopped
};

template<event type>
struct event_trait;

template<event EventType>
concept supported_event_callback = requires { typename event_trait<EventType>::callback; };

//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------

class brypt::service
{
public:
    enum class peer_filter : std::uint32_t { active, inactive, observed };

    using request_fulfilled_callback = std::function<void(request_key)>;

    class request_store
    {
    public:
        response_callback on_response;
        request_error_callback on_error;
        request_fulfilled_callback on_fulfilled;
    };

    service();
    explicit service(brypt::result& result);
    explicit service(std::string_view base_path);
    service(std::string_view base_path, brypt::result& result);
    ~service();

    service(service const& other) = delete;
    service(service&& other) = default;
    service& operator=(service const& other) = delete;
    service& operator=(service&& other) = default;

    [[nodiscard]] identifier const& get_identifier() const noexcept;

    result startup() noexcept;
    result shutdown() noexcept;

    [[nodiscard]] option get_option(brypt_option_t name) const;
    [[nodiscard]] option get_option(brypt_option_t name, result& result) const noexcept;

    template<supported_option_type ValueType>
    [[nodiscard]] ValueType get_option(brypt_option_t name) const;

    result set_option(option const& opt) noexcept;
    result set_option(brypt_option_t name, std::string_view value);

    template<supported_primitive_type ValueType>
    result set_option(brypt_option_t name, ValueType value);

    template<typename Rep, typename Period>
    result set_option(brypt_option_t name, std::chrono::duration<Rep, Period> const& value);

    [[nodiscard]] std::vector<endpoint_options> get_endpoints() const;
    [[nodiscard]] std::optional<endpoint_options> find_endpoint(protocol protocol, std::string_view binding) const;
    result attach_endpoint(endpoint_options const& options);
    result detach_endpoint(protocol protocol, std::string_view binding);

    [[nodiscard]] bool is_active() const noexcept;

    [[nodiscard]] bool is_peer_connected(identifier const& identifier) const;
    [[nodiscard]] std::optional<peer_statistics> get_peer_statistics(identifier const& identifier) const;
    [[nodiscard]] std::optional<peer_details> get_peer_details(identifier const& identifier) const;
    
    [[nodiscard]] std::size_t active_peers() const noexcept;
    [[nodiscard]] std::size_t inactive_peers() const noexcept;
    [[nodiscard]] std::size_t observed_peers() const noexcept;

    result route(std::string_view route, message_callback const& on_message);

    result connect(protocol protocol, std::string_view address) const noexcept;
    result disconnect(identifier const& identifier) const noexcept;
    std::size_t disconnect(protocol protocol, std::string_view address) const noexcept;

    result dispatch(identifier const& identifier, std::string_view route, std::span<std::uint8_t const> payload) const;
    std::optional<std::size_t> cluster_dispatch(std::string_view route, std::span<std::uint8_t const> payload) const;
    std::optional<std::size_t> sample_dispatch(std::string_view route, std::span<std::uint8_t const> payload, double sample) const;

    result request(
        identifier const& identifier,
        std::string_view route,
        std::span<std::uint8_t const> payload,
        response_callback const& on_response,
        request_error_callback const& on_error);

    std::optional<std::size_t> cluster_request(
        std::string_view route,
        std::span<std::uint8_t const> payload,
        response_callback const& on_response,
        request_error_callback const& on_error);

    std::optional<std::size_t> sample_request(
        std::string_view route,
        std::span<std::uint8_t const> payload,
        double sample,
        response_callback const& on_response,
        request_error_callback const& on_error);

    template<event EventType> requires supported_event_callback<EventType>
    result subscribe(typename event_trait<EventType>::callback const& callback);

    result register_logger(log_callback const& watcher);

private:
    using unique_service_t = std::unique_ptr<brypt_service_t, decltype(&brypt_service_destroy)>;

    [[nodiscard]] result fetch_identifier();

    unique_service_t m_service;
    identifier m_identifier;

    std::forward_list<std::any> m_callbacks;

    mutable std::mutex m_requests_mutex;
    std::map<request_key, std::unique_ptr<request_store>> m_requests;

    log_callback m_logger;
};

//----------------------------------------------------------------------------------------------------------------------
// brypt::service {
//----------------------------------------------------------------------------------------------------------------------

inline brypt::service::service()
    : m_service(brypt_service_create(), &brypt_service_destroy)
    , m_identifier()
    , m_callbacks()
    , m_requests_mutex()
    , m_requests()
{
    if (!m_service) { throw brypt::result{ result_code::initialization_failure }; }

    brypt::result result = fetch_identifier();
    if (result.is_error()) { throw result; }
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::service::service(brypt::result& result)
    : m_service(brypt_service_create(), &brypt_service_destroy)
    , m_identifier()
    , m_callbacks()
    , m_requests_mutex()
    , m_requests()
{
    if (!m_service) { result = result_code::initialization_failure; return; }
    result = fetch_identifier();
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::service::service(std::string_view base_path)
    : m_service(brypt_service_create(), &brypt_service_destroy)
    , m_identifier()
    , m_callbacks()
    , m_requests_mutex()
    , m_requests()
{
    brypt::result result; 
    if (!m_service) { throw brypt::result{ result_code::initialization_failure }; }
    
    result = brypt_option_set_string(m_service.get(), BRYPT_OPT_BASE_FILEPATH, base_path.data(), base_path.size());
    if (result.is_error()) { throw result; }

    result = fetch_identifier();
    if (result.is_error()) { throw result; }
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::service::service(std::string_view base_path, brypt::result& result)
    : m_service(brypt_service_create(), &brypt_service_destroy)
    , m_identifier()
    , m_callbacks()
    , m_requests_mutex()
    , m_requests()
{
    if (!m_service) { result = result_code::initialization_failure; return; }
    result = brypt_option_set_string(m_service.get(), BRYPT_OPT_BASE_FILEPATH, base_path.data(), base_path.size());
    if (result.is_error()) { return; }

    result = fetch_identifier();
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::service::~service()
{
    if (m_service) { [[maybe_unused]] auto const result = shutdown(); }
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier const& brypt::service::get_identifier() const noexcept { return m_identifier; }

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::startup() noexcept
{
    assert(m_service);
    return brypt_service_start(m_service.get());
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::shutdown() noexcept
{
    assert(m_service);
    return brypt_service_stop(m_service.get());
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::option brypt::service::get_option(brypt_option_t name) const
{
    assert(m_service);

    switch (name) {
        case option::use_bootstraps: {
            return option{ name, brypt_option_get_bool(m_service.get(), name) };
        }
        case option::core_threads:
        case option::connection_retry_limit: {
            return option{ name, brypt_option_get_int32(m_service.get(), name) };
        }
        case option::identifier_type: {
            return option{ name, static_cast<identifier_type>(brypt_option_get_int32(m_service.get(), name)) };
        }
        case option::security_strategy: {
            return option{ name, static_cast<security_strategy>(brypt_option_get_int32(m_service.get(), name)) };
        }
        case option::log_level: {
            return option{ name, static_cast<log_level>(brypt_option_get_int32(m_service.get(), name)) };
        }
        case option::connection_timeout:
        case option::connection_retry_interval: {
            return option{ name, std::chrono::milliseconds{ brypt_option_get_int32(m_service.get(), name) } };
        }
        case option::base_path:
        case option::configuration_filename:
        case option::bootstrap_filename:
        case option::node_name:
        case option::node_description: {
            return option{ name, brypt_option_get_string(m_service.get(), name) };
        } 
        default: break;
    }

    throw brypt::result{ result_code::invalid_argument };
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::option brypt::service::get_option(brypt_option_t name, result& result) const noexcept
{
    assert(m_service);

    result = result_code::accepted;

    switch (name) {
        case option::use_bootstraps: {
            return option{ name, brypt_option_get_bool(m_service.get(), name) };
        }
        case option::core_threads: 
        case option::connection_retry_limit: {
            return option{ name, brypt_option_get_int32(m_service.get(), name) };
        }
        case option::identifier_type: {
            return option{ name, static_cast<identifier_type>(brypt_option_get_int32(m_service.get(), name)) };
        }
        case option::security_strategy: {
            return option{ name, static_cast<security_strategy>(brypt_option_get_int32(m_service.get(), name)) };
        }
        case option::log_level: {
            return option{ name, static_cast<log_level>(brypt_option_get_int32(m_service.get(), name)) };
        }
        case option::connection_timeout:
        case option::connection_retry_interval: {
            return option{ name, std::chrono::milliseconds{ brypt_option_get_int32(m_service.get(), name) } };
        }
        case option::base_path:
        case option::configuration_filename:
        case option::bootstrap_filename:
        case option::node_name:
        case option::node_description: {
            return option{ name, brypt_option_get_string(m_service.get(), name) };
        }
        default: break;
    }

    result = result_code::invalid_argument;
}

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_option_type ValueType>
inline ValueType brypt::service::get_option(brypt_option_t name) const
{
    assert(m_service);

    if constexpr (std::is_same_v<ValueType, bool>) {
        switch (name) {
            case option::use_bootstraps: {
                return brypt_option_get_bool(m_service.get(), name);
            }
            default: break;
        }
    } else if constexpr (std::is_same_v<ValueType, std::int32_t>) {
        switch (name) {
            case option::core_threads: 
            case option::identifier_type:
            case option::security_strategy:
            case option::log_level:
            case option::connection_timeout:
            case option::connection_retry_limit:
            case option::connection_retry_interval: {
                return brypt_option_get_int32(m_service.get(), name);
            }
            default: break;
        }
    } else if constexpr (std::is_enum_v<ValueType>) {
        if constexpr (std::is_same_v<std::underlying_type_t<ValueType>, std::int32_t>) {
            switch (name) {
                case option::identifier_type:
                case option::security_strategy:
                case option::log_level: {
                    return static_cast<ValueType>(brypt_option_get_int32(m_service.get(), name));
                }
                default: break;
            }
        }
    } else if constexpr (std::is_same_v<ValueType, std::chrono::milliseconds>) {
        switch (name) {
            case option::connection_timeout:
            case option::connection_retry_interval: {
                return std::chrono::milliseconds{ brypt_option_get_int32(m_service.get(), name) };
            }
            default: break;
        }
    } else if constexpr (std::is_same_v<ValueType, std::string>) {
        switch (name) {
            case option::base_path:
            case option::configuration_filename:
            case option::bootstrap_filename:
            case option::node_name:
            case option::node_description: {
                return brypt_option_get_string(m_service.get(), name);
            }
            default: break;
        }
    } 

    // Option handlers should only reach this point if an option was coupled with an invalid value type.
    throw brypt::result{ result_code::invalid_argument };
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::set_option(option const& opt) noexcept
{
    assert(m_service);

    brypt::result result{ result_code::invalid_argument };
    switch (opt.name()) {
        case option::use_bootstraps: if (opt.contains<bool>()) {
            result = brypt_option_set_bool(m_service.get(), opt.name(), opt.value<bool>());
        } break;
        case option::core_threads: 
        case option::connection_retry_limit: if (opt.contains<std::int32_t>()) {
            result = brypt_option_set_int32(m_service.get(), opt.name(), opt.value<std::int32_t>());
        } break;
        case option::identifier_type: {
            if (opt.contains<std::int32_t>()) {
                result = brypt_option_set_int32(m_service.get(), opt.name(), opt.value<std::int32_t>());
            } else if (opt.contains<identifier_type>()) {
                auto const& level = opt.value<identifier_type>();
                result = brypt_option_set_int32(m_service.get(), opt.name(), static_cast<std::int32_t>(level));
            }
            if (result.is_success()) { result = fetch_identifier(); }
        } break;
        case option::security_strategy: {
            if (opt.contains<std::int32_t>()) {
                result = brypt_option_set_int32(m_service.get(), opt.name(), opt.value<std::int32_t>());
            } else if (opt.contains<security_strategy>()) {
                auto const& level = opt.value<security_strategy>();
                result = brypt_option_set_int32(m_service.get(), opt.name(), static_cast<std::int32_t>(level));
            }
        } break;
        case option::log_level: {
            if (opt.contains<std::int32_t>()) {
                result = brypt_option_set_int32(m_service.get(), opt.name(), opt.value<std::int32_t>());
            } else if (opt.contains<log_level>()) {
                auto const& level = opt.value<log_level>();
                result = brypt_option_set_int32(m_service.get(), opt.name(), static_cast<std::int32_t>(level));
            }
        } break;
        case option::connection_timeout:
        case option::connection_retry_interval: {
            if (opt.contains<std::int32_t>()) {
                result = brypt_option_set_int32(m_service.get(), opt.name(), opt.value<std::int32_t>());
            } else if (opt.contains<std::chrono::milliseconds>()) {
                auto const& milliseconds = opt.value<std::chrono::milliseconds>();
                if (!std::in_range<std::int32_t>(milliseconds.count())) {
                    return result_code::invalid_argument;
                }

                result = brypt_option_set_int32(
                    m_service.get(), opt.name(), static_cast<std::int32_t>(milliseconds.count()));
            }
        } break;
        case option::base_path:
        case option::configuration_filename:
        case option::bootstrap_filename:
        case option::node_name:
        case option::node_description: if (opt.contains<std::string>()) {
            auto const& value = opt.value<std::string>();
            result = brypt_option_set_string(m_service.get(), opt.name(), value.c_str(), value.size());
            if (result.is_success() && opt.name() == option::configuration_filename) {
                result = fetch_identifier();
            }
        } break;
        default: break;
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::set_option(brypt_option_t name, std::string_view value)
{
    assert(m_service);

    brypt::result result{ result_code::invalid_argument };

    switch (name) {
        case option::base_path:
        case option::configuration_filename:
        case option::bootstrap_filename:
        case option::node_name:
        case option::node_description: {
            result = brypt_option_set_string(m_service.get(), name, value.data(), value.size());
            if (result.is_success() && name == option::configuration_filename) {
                result = fetch_identifier();
            }
        } break;
        default: break;
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

template<brypt::supported_primitive_type ValueType>
inline brypt::result brypt::service::set_option(brypt_option_t name, ValueType value)
{
    assert(m_service);

    brypt::result result{ result_code::invalid_argument };
    if constexpr (std::is_same_v<ValueType, bool>) {
        switch (name) {
            case option::use_bootstraps: {
                result = brypt_option_set_bool(m_service.get(), name, value);
            } break;
            default: break;
        }
    } else if constexpr (std::is_same_v<ValueType, std::int32_t>) {
        switch (name) {
            case option::core_threads:
            case option::identifier_type:
            case option::security_strategy:
            case option::log_level:
            case option::connection_timeout:
            case option::connection_retry_limit:
            case option::connection_retry_interval: {
                result = brypt_option_set_int32(m_service.get(), name, value);
                if (result.is_success() && name == option::identifier_type) { 
                    result = fetch_identifier(); 
                }
            } break;
            default: break;
        }
    } else if constexpr (std::is_enum_v<ValueType>) {
        if constexpr (std::is_same_v<std::underlying_type_t<ValueType>, std::int32_t>) {
            auto const casted = static_cast<std::int32_t>(value);
            switch (name) {
                case option::identifier_type:
                case option::security_strategy:
                case option::log_level: {
                    result = brypt_option_set_int32(m_service.get(), name, casted);
                    if (result.is_success() && name == option::identifier_type) {
                        result = fetch_identifier();
                    }
                } break;
                default: break;
            }
        }
    } else if constexpr (std::is_same_v<ValueType, std::chrono::milliseconds>) {
        switch (name) {
            case option::connection_timeout:
            case option::connection_retry_interval: {
                if (!std::in_range<std::int32_t>(value.count())) {
                    return result_code::invalid_argument;
                }

                result = brypt_option_set_int32(m_service.get(), name, static_cast<std::int32_t>(value.count()));
            } break;
            default: break;
        }
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename Rep, typename Period>
brypt::result brypt::service::set_option(brypt_option_t name, std::chrono::duration<Rep, Period> const& value)
{
    assert(m_service);

    brypt::result result{ result_code::invalid_argument };

    switch (name) {
        case option::connection_timeout:
        case option::connection_retry_interval: {
            auto const milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(value);
            if (!std::in_range<std::int32_t>(milliseconds.count())) {
                return result_code::invalid_argument;
            }

            result = brypt_option_set_int32(m_service.get(), name, static_cast<std::int32_t>(milliseconds.count()));
        } break;
        default: break;
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

extern "C" inline bool brypt_read_endpoints_wrapper(brypt_option_endpoint_t const* const options, void* context)
{
    auto const endpoints = static_cast<std::vector<brypt::endpoint_options>* const>(context);
    assert(endpoints);
    endpoints->emplace_back(options);
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::vector<brypt::endpoint_options> brypt::service::get_endpoints() const 
{
    assert(m_service);
    std::vector<endpoint_options> endpoints;
    brypt_option_read_endpoints(m_service.get(), brypt_read_endpoints_wrapper, &endpoints);
    return endpoints;
}

//----------------------------------------------------------------------------------------------------------------------

extern "C" inline bool brypt_find_endpoints_wrapper(brypt_option_endpoint_t const* const options, void* context)
{
    auto const endpoint = static_cast<brypt::endpoint_options* const>(context);
    assert(endpoint);
    *endpoint = brypt::endpoint_options{ options };
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<brypt::endpoint_options> brypt::service::find_endpoint(
    brypt::protocol protocol, std::string_view binding) const
{
    endpoint_options endpoint;
    brypt_protocol_t _protocol = static_cast<brypt_protocol_t>(protocol);
    brypt::result const result = brypt_option_find_endpoint(
        m_service.get(), _protocol, binding.data(), brypt_find_endpoints_wrapper, &endpoint);
    if (!result) { return {}; }
    return endpoint;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::attach_endpoint(endpoint_options const& options)
{
    assert(m_service);
    brypt_option_endpoint_t endpoint = options;
    return brypt_option_attach_endpoint(m_service.get(), &endpoint);
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::detach_endpoint(brypt::protocol protocol, std::string_view binding)
{
    assert(m_service);
    return brypt_option_detach_endpoint(m_service.get(), static_cast<std::int32_t>(protocol), binding.data());
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::service::is_active() const noexcept
{
    assert(m_service);
    return brypt_service_is_active(m_service.get());
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::service::is_peer_connected(identifier const& identifier) const
{
    assert(m_service);
    return brypt_service_is_peer_connected(m_service.get(), static_cast<std::string const&>(identifier).c_str());
}

//----------------------------------------------------------------------------------------------------------------------

extern "C" inline bool brypt_get_peer_statistics_wrapper(brypt_peer_statistics_t const* const _statistics, void* context)
{
    auto const statistics = static_cast<brypt::peer_statistics* const>(context);
    assert(statistics && _statistics);
    *statistics = brypt::peer_statistics{ *_statistics };
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<brypt::peer_statistics> brypt::service::get_peer_statistics(identifier const& identifier) const
{
    peer_statistics statistics;
    brypt::result const result = brypt_service_get_peer_statistics(
        m_service.get(),
        static_cast<std::string const&>(identifier).c_str(),
        brypt_get_peer_statistics_wrapper,
        &statistics);
    if (!result) { return {}; }
    return statistics;
}

//----------------------------------------------------------------------------------------------------------------------

extern "C" inline bool brypt_get_peer_details_wrapper(brypt_peer_details_t const* const _details, void* context)
{
    auto const details = static_cast<brypt::peer_details* const>(context);
    assert(details && _details);
    *details = brypt::peer_details{ *_details };
    return true;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<brypt::peer_details> brypt::service::get_peer_details(identifier const& identifier) const
{
    peer_details details;
    brypt::result const result = brypt_service_get_peer_details(
        m_service.get(),
        static_cast<std::string const&>(identifier).c_str(),
        brypt_get_peer_details_wrapper,
        &details);
    if (!result) { return {}; }
    return details;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::size_t brypt::service::active_peers() const noexcept
{
    assert(m_service);
    return brypt_service_active_peer_count(m_service.get());
}

//----------------------------------------------------------------------------------------------------------------------

inline std::size_t brypt::service::inactive_peers() const noexcept
{
    assert(m_service);
    return brypt_service_inactive_peer_count(m_service.get());
}

//----------------------------------------------------------------------------------------------------------------------

inline std::size_t brypt::service::observed_peers() const noexcept
{
    assert(m_service);
    return brypt_service_observed_peer_count(m_service.get());
}

//----------------------------------------------------------------------------------------------------------------------

extern "C" inline bool brypt_on_message_wrapper(
    char const* source, uint8_t const* payload, size_t size, brypt_next_key_t* next, void* context)
{
    assert(context);
    using store_t = brypt::message_callback;
    auto const* const store = static_cast<store_t*>(context);
    return std::invoke(*store, source, std::span{ payload, size }, brypt::next{ next });
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::route(std::string_view route, message_callback const& on_message)
{
    assert(m_service);
    std::any& store = m_callbacks.emplace_front(on_message);
    assert(std::any_cast<message_callback>(&store));
    return brypt_service_register_route(
        m_service.get(), route.data(), brypt_on_message_wrapper, std::any_cast<message_callback>(&store));
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::connect(brypt::protocol protocol, std::string_view address) const noexcept
{
    assert(m_service);
    if (protocol == brypt::protocol::unknown || address.empty()) { return result_code::invalid_argument; }
    return brypt_service_connect(m_service.get(), static_cast<std::int32_t>(protocol), address.data());
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::disconnect(brypt::identifier const& identifier) const noexcept
{
    assert(m_service);
    if (identifier.empty()) { return result_code::invalid_argument; }
    return brypt_service_disconnect_by_identifier(m_service.get(), static_cast<std::string const&>(identifier).c_str());
}

//----------------------------------------------------------------------------------------------------------------------

inline std::size_t brypt::service::disconnect(brypt::protocol protocol, std::string_view address) const noexcept
{
    assert(m_service);
    if (protocol == brypt::protocol::unknown || address.empty()) { return std::numeric_limits<std::size_t>::min(); }
    return brypt_service_disconnect_by_address(m_service.get(), static_cast<std::int32_t>(protocol), address.data());
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::dispatch(
    brypt::identifier const& identifier, std::string_view route, std::span<std::uint8_t const> payload) const
{
    assert(m_service);
    auto const _identifier = static_cast<std::string const&>(identifier).c_str();
    auto const result = brypt_service_dispatch(
        m_service.get(), _identifier, route.data(), payload.data(), payload.size());
    return result.result;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<std::size_t> brypt::service::cluster_dispatch(
    std::string_view route, std::span<std::uint8_t const> payload) const
{
    assert(m_service);
    auto const result = brypt_service_dispatch_cluster(
        m_service.get(), route.data(), payload.data(), payload.size());
    if (result.result != BRYPT_ACCEPTED) { return {}; }
    return result.dispatched;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<std::size_t> brypt::service::sample_dispatch(
    std::string_view route, std::span<std::uint8_t const> payload, double sample) const
{
    assert(m_service);
    auto const result = brypt_service_dispatch_cluster_sample(
        m_service.get(), route.data(), payload.data(), payload.size(), sample);
    if (result.result != BRYPT_ACCEPTED) { return {}; }
    return result.dispatched;
}

//----------------------------------------------------------------------------------------------------------------------

extern "C" inline void brypt_on_response_wrapper(brypt_response_t const* const _response, void* context)
{
    assert(context);
    auto const* const store = static_cast<brypt::service::request_store*>(context);
    std::invoke(store->on_response, brypt::response{ *_response });
    if (_response->remaining == 0) {
        std::invoke(store->on_fulfilled, brypt::request_key{ _response->key });
    }
}

//----------------------------------------------------------------------------------------------------------------------

extern "C" inline void brypt_on_request_error_wrapper(brypt_response_t const* const _response, void* context)
{
    assert(context);
    auto const* const store = static_cast<brypt::service::request_store*>(context);
    std::invoke(store->on_error, brypt::response{ *_response });
    if (_response->remaining == 0) {
        std::invoke(store->on_fulfilled, brypt::request_key{ _response->key });
    }
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::request(
    brypt::identifier const& identifier,
    std::string_view route,
    std::span<std::uint8_t const> payload,
    response_callback const& on_response,
    request_error_callback const& on_error)
{
    assert(m_service);
 
    auto const _identifier = static_cast<std::string const&>(identifier).c_str();
    auto store = std::make_unique<request_store>(on_response, on_error, [this] (request_key key) { 
        std::scoped_lock lock{ m_requests_mutex };
        m_requests.erase(key);
    });

    auto const result = brypt_service_request(
        m_service.get(), _identifier, route.data(), payload.data(), payload.size(),
        brypt_on_response_wrapper, brypt_on_request_error_wrapper, store.get());
    if (result.result != BRYPT_ACCEPTED) { return result.result; }

    std::scoped_lock lock{ m_requests_mutex };
    m_requests.emplace(result.key, std::move(store));

    return result_code::accepted;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<std::size_t> brypt::service::cluster_request(
    std::string_view route,
    std::span<std::uint8_t const> payload,
    response_callback const& on_response,
    request_error_callback const& on_error)
{
    assert(m_service);

    auto store = std::make_unique<request_store>(on_response, on_error, [this] (request_key key) { 
        std::scoped_lock lock{ m_requests_mutex };
        m_requests.erase(key);
    });

    auto const result = brypt_service_request_cluster(
        m_service.get(), route.data(), payload.data(), payload.size(), 
        brypt_on_response_wrapper, brypt_on_request_error_wrapper, store.get());
    if (result.result != BRYPT_ACCEPTED) { return {}; }

    if (result.requested != 0) {
        std::scoped_lock lock{ m_requests_mutex };
        m_requests.emplace(result.key, std::move(store));
    }

    return result.requested;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::optional<std::size_t> brypt::service::sample_request(
    std::string_view route,
    std::span<std::uint8_t const> payload,
    double sample,
    response_callback const& on_response,
    request_error_callback const& on_error)
{
    assert(m_service);

    auto store = std::make_unique<request_store>(on_response, on_error, [this] (request_key key) { 
        std::scoped_lock lock{ m_requests_mutex };
        m_requests.erase(key);
    });

    auto const result = brypt_service_request_cluster_sample(
        m_service.get(), route.data(), payload.data(), payload.size(), sample,
        brypt_on_response_wrapper, brypt_on_request_error_wrapper, store.get());
    if (result.result != BRYPT_ACCEPTED) { return {}; }

    if (result.requested != 0) {
        std::scoped_lock lock{ m_requests_mutex };
        m_requests.emplace(result.key, std::move(store));
    }
    
    return result.requested;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::fetch_identifier()
{
    std::string external;
    external.resize(BRYPT_IDENTIFIER_MAX_SIZE, '\x00');
    std::size_t written = brypt_service_get_identifier(m_service.get(), external.data(), external.size());
    if (written < BRYPT_IDENTIFIER_MIN_SIZE) { return result_code::initialization_failure; }
    external.resize(written);
    m_identifier = std::move(external);
    return brypt::result{ result_code::accepted };
}

//----------------------------------------------------------------------------------------------------------------------

#define BRYPT_DEFINE_EVENT_TRAIT(event_type, ...)\
template<> \
struct brypt::event_trait<brypt::event::event_type> \
{ \
    using callback = std::function<void(__VA_ARGS__)>; \
};

#define BRYPT_EVENT_ABI_WRAPPER(callback_name, ...) \
extern "C" inline void brypt_on_##callback_name##_wrapper(__VA_ARGS__ __VA_OPT__(,) void* context)

#define BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(event_type)\
template<> inline \
[[nodiscard]] brypt::result brypt::service::subscribe<brypt::event::event_type>(\
    typename event_trait<event::event_type>::callback const& callback) \
{ \
    assert(m_service && callback); \
    using store_t = event_trait<event::event_type>::callback; \
    std::any& store = m_callbacks.emplace_front(callback); \
    assert(std::any_cast<store_t>(&store)); \
    return brypt_event_subscribe_##event_type( \
        m_service.get(), brypt_on_##event_type##_wrapper, std::any_cast<store_t>(&store)); \
}

//----------------------------------------------------------------------------------------------------------------------

BRYPT_DEFINE_EVENT_TRAIT(binding_failed, brypt::protocol protocol, std::string_view uri, brypt::result const& result)

BRYPT_EVENT_ABI_WRAPPER(binding_failed, brypt_protocol_t protocol, char const* uri, brypt_result_t result)
{
    assert(context);
    using store_t = brypt::event_trait<brypt::event::binding_failed>::callback;
    auto const* const store = static_cast<store_t*>(context);
    std::invoke(*store, static_cast<brypt::protocol>(protocol), uri, brypt::result{ result });
}

BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(binding_failed)

//----------------------------------------------------------------------------------------------------------------------

BRYPT_DEFINE_EVENT_TRAIT(connection_failed, brypt::protocol protocol, std::string_view uri, brypt::result const& result)

BRYPT_EVENT_ABI_WRAPPER(connection_failed, brypt_protocol_t protocol, char const* uri, brypt_result_t result)
{
    assert(context);
    using store_t = brypt::event_trait<brypt::event::connection_failed>::callback;
    auto const* const store = static_cast<store_t*>(context);
    std::invoke(*store, static_cast<brypt::protocol>(protocol), uri, brypt::result{ result });
}

BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(connection_failed)

//----------------------------------------------------------------------------------------------------------------------

BRYPT_DEFINE_EVENT_TRAIT(endpoint_started, brypt::protocol protocol, std::string_view uri)

BRYPT_EVENT_ABI_WRAPPER(endpoint_started, brypt_protocol_t protocol, char const* uri)
{
    assert(context);
    using store_t = brypt::event_trait<brypt::event::endpoint_started>::callback;
    auto const* const store = static_cast<store_t*>(context);
    std::invoke(*store, static_cast<brypt::protocol>(protocol), uri);
}

BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(endpoint_started)

//----------------------------------------------------------------------------------------------------------------------

BRYPT_DEFINE_EVENT_TRAIT(
    endpoint_stopped, brypt::protocol protocol, std::string_view uri, brypt::result const& result)

BRYPT_EVENT_ABI_WRAPPER(
    endpoint_stopped, brypt_protocol_t protocol, char const* uri, brypt_result_t result)
{
    assert(context);
    using store_t = brypt::event_trait<brypt::event::endpoint_stopped>::callback;
    auto const* const store = static_cast<store_t*>(context);
    std::invoke(*store, static_cast<brypt::protocol>(protocol), uri, brypt::result{ result });
}

BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(endpoint_stopped)

//----------------------------------------------------------------------------------------------------------------------

BRYPT_DEFINE_EVENT_TRAIT(peer_connected, std::string_view identifier, brypt::protocol protocol)

BRYPT_EVENT_ABI_WRAPPER(peer_connected, char const* identifier, brypt_protocol_t protocol)
{
    assert(context);
    using store_t = brypt::event_trait<brypt::event::peer_connected>::callback;
    auto const* const store = static_cast<store_t*>(context);
    std::invoke(*store, identifier, static_cast<brypt::protocol>(protocol));
}

BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(peer_connected)

//----------------------------------------------------------------------------------------------------------------------

BRYPT_DEFINE_EVENT_TRAIT(
    peer_disconnected, std::string_view identifier, brypt::protocol protocol, brypt::result const& result)

BRYPT_EVENT_ABI_WRAPPER(
    peer_disconnected, char const* identifier, brypt_protocol_t protocol, brypt_result_t result)
{
    assert(context);
    using store_t = brypt::event_trait<brypt::event::peer_disconnected>::callback;
    auto const* const store = static_cast<store_t*>(context);
    std::invoke(*store, identifier, static_cast<brypt::protocol>(protocol), brypt::result{ result });
}

BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(peer_disconnected)

//----------------------------------------------------------------------------------------------------------------------

BRYPT_DEFINE_EVENT_TRAIT(runtime_started)

BRYPT_EVENT_ABI_WRAPPER(runtime_started)
{
    assert(context);
    using store_t = brypt::event_trait<brypt::event::runtime_started>::callback;
    auto const* const store = static_cast<store_t*>(context);
    std::invoke(*store);
}

BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(runtime_started)

//----------------------------------------------------------------------------------------------------------------------

BRYPT_DEFINE_EVENT_TRAIT(runtime_stopped, brypt::result const& result)

BRYPT_EVENT_ABI_WRAPPER(runtime_stopped, brypt_result_t result)
{
    assert(context);
    using store_t = brypt::event_trait<brypt::event::runtime_stopped>::callback;
    auto const* const store = static_cast<store_t*>(context);
    std::invoke(*store, brypt::result{ result });
}

BRYPT_DEFINE_EVENT_SUBSCRIPTION_PASSTHROUGH(runtime_stopped)

//----------------------------------------------------------------------------------------------------------------------

extern "C" inline void brypt_on_log_message_wrapper(
    brypt_log_level_t level, char const* message, size_t size, void* context)
{
    assert(context);
    auto const* const store = static_cast<brypt::log_callback*>(context);
    std::invoke(*store, static_cast<brypt::log_level>(level), std::string_view{ message, size });
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::service::register_logger(log_callback const& logger)
{
    assert(m_service);
    m_logger = logger;
    return brypt_service_register_logger(m_service.get(), brypt_on_log_message_wrapper, &m_logger);
}

//----------------------------------------------------------------------------------------------------------------------
// } brypt::service
//----------------------------------------------------------------------------------------------------------------------
