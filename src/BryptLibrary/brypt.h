//----------------------------------------------------------------------------------------------------------------------
//------------------------------------             Brypt Library Interface             ---------------------------------
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
#include <cerrno>
#include <cstdint>
#include <cstddef>
#else
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#endif
//----------------------------------------------------------------------------------------------------------------------

#define BRYPT_VERSION_MAJOR 0
#define BRYPT_VERSION_MINOR 0
#define BRYPT_VERSION_PATCH 0
#define BRYPT_VERSION \
    ((BRYPT_VERSION_MAJOR * 10000) + (BRYPT_VERSION_MINOR * 100) + BRYPT_VERSION_PATCH)

#define BRYPT_VERSION_STR "0.0.0"

//----------------------------------------------------------------------------------------------------------------------

#if defined(_WIN32) && !defined(__GNUC__)
    #if defined(BRYPT_SHARED)
        #define BRYPT_EXPORT __declspec(dllexport)
    #else
        #define BRYPT_EXPORT __declspec(dllimport)
    #endif
    #define BRYPT_LOCAL
#else
    #if defined(BRYPT_SHARED) && (__GNUC__ >= 4)
        #define BRYPT_EXPORT __attribute__ ((visibility("default")))
        #define BRYPT_LOCAL  __attribute__ ((visibility("hidden")))
    #else
        #define BRYPT_EXPORT
        #define BRYPT_LOCAL
    #endif
#endif

//----------------------------------------------------------------------------------------------------------------------

#if defined(__cplusplus)
    #define BRYPT_CONSTANT constexpr
#else
    #define BRYPT_CONSTANT const
#endif

//----------------------------------------------------------------------------------------------------------------------

#if defined(__cplusplus)
    #define BRYPT_NOEXCEPT noexcept
#else
    #define BRYPT_NOEXCEPT 
#endif

//----------------------------------------------------------------------------------------------------------------------

#if defined(__cplusplus) && !defined(_Static_assert)
    #define _Static_assert static_assert
#endif
#define BRYPT_STATIC_ASSERT(test, reason) _Static_assert((test), #reason)

//----------------------------------------------------------------------------------------------------------------------

#define BRYPT_DECLARE_HANDLE(name) struct brypt_##name; typedef struct brypt_##name##_t brypt_##name##_t

//----------------------------------------------------------------------------------------------------------------------

#define BRYPT_CATEGORY_VALUE(category, value)  (category * 128) + value; \
    BRYPT_STATIC_ASSERT(category < 51, "Brypt category exceeded maximum range!"); \
    BRYPT_STATIC_ASSERT(value < 128, "Brypt constant exceeded maximum category range!")

#define BRYPT_GENERIC_VALUE(value) BRYPT_CATEGORY_VALUE(0, value)
#define BRYPT_SERVICE_VALUE(value) BRYPT_CATEGORY_VALUE(1, value)
#define BRYPT_CONFIGURATION_VALUE(value) BRYPT_CATEGORY_VALUE(2, value)
#define BRYPT_NETWORK_VALUE(value) BRYPT_CATEGORY_VALUE(3, value)

//----------------------------------------------------------------------------------------------------------------------

typedef uint32_t brypt_option_t;

BRYPT_CONSTANT brypt_option_t BRYPT_OPT_BASE_FILEPATH = BRYPT_CONFIGURATION_VALUE(1);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_CONFIGURATION_FILENAME = BRYPT_CONFIGURATION_VALUE(2);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_BOOTSTRAP_FILENAME = BRYPT_CONFIGURATION_VALUE(3);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_CORE_THREADS = BRYPT_CONFIGURATION_VALUE(10);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_IDENTIFIER_TYPE = BRYPT_CONFIGURATION_VALUE(16);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_USE_BOOTSTRAPS = BRYPT_CONFIGURATION_VALUE(17);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_NODE_NAME = BRYPT_CONFIGURATION_VALUE(32);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_NODE_DESCRIPTION = BRYPT_CONFIGURATION_VALUE(33);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_SECURITY_STRATEGY = BRYPT_CONFIGURATION_VALUE(64);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_LOG_LEVEL = BRYPT_CONFIGURATION_VALUE(80);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_CONNECTION_TIMEOUT = BRYPT_NETWORK_VALUE(16);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_CONNECTION_RETRY_LIMIT = BRYPT_NETWORK_VALUE(17);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_CONNECTION_RETRY_INTERVAL = BRYPT_NETWORK_VALUE(18);

//----------------------------------------------------------------------------------------------------------------------

typedef uint32_t brypt_result_t;

BRYPT_CONSTANT brypt_result_t BRYPT_ACCEPTED = 0;

BRYPT_CONSTANT brypt_result_t BRYPT_ECANCELED = BRYPT_GENERIC_VALUE(1);
BRYPT_CONSTANT brypt_result_t BRYPT_ESHUTDOWNREQUESTED = BRYPT_GENERIC_VALUE(2);
BRYPT_CONSTANT brypt_result_t BRYPT_EINVALIDARGUMENT = BRYPT_GENERIC_VALUE(5);
BRYPT_CONSTANT brypt_result_t BRYPT_EACCESSDENIED = BRYPT_GENERIC_VALUE(6);
BRYPT_CONSTANT brypt_result_t BRYPT_EINPROGRESS = BRYPT_GENERIC_VALUE(7);
BRYPT_CONSTANT brypt_result_t BRYPT_ETIMEOUT = BRYPT_GENERIC_VALUE(8);
BRYPT_CONSTANT brypt_result_t BRYPT_ECONFLICT = BRYPT_GENERIC_VALUE(9);
BRYPT_CONSTANT brypt_result_t BRYPT_EMISSINGFIELD = BRYPT_CONFIGURATION_VALUE(10);
BRYPT_CONSTANT brypt_result_t BRYPT_EPAYLOADTOOLARGE = BRYPT_GENERIC_VALUE(11);
BRYPT_CONSTANT brypt_result_t BRYPT_EOUTOFMEMORY = BRYPT_GENERIC_VALUE(12);
BRYPT_CONSTANT brypt_result_t BRYPT_ENOTFOUND = BRYPT_SERVICE_VALUE(13);
BRYPT_CONSTANT brypt_result_t BRYPT_ENOTAVAILABLE = BRYPT_SERVICE_VALUE(14);
BRYPT_CONSTANT brypt_result_t BRYPT_ENOTSUPPORTED = BRYPT_GENERIC_VALUE(15);
BRYPT_CONSTANT brypt_result_t BRYPT_EUNSPECIFIED = BRYPT_GENERIC_VALUE(126);
BRYPT_CONSTANT brypt_result_t BRYPT_ENOTIMPLEMENTED = BRYPT_GENERIC_VALUE(127);

BRYPT_CONSTANT brypt_result_t BRYPT_EINITFAILURE = BRYPT_SERVICE_VALUE(1);
BRYPT_CONSTANT brypt_result_t BRYPT_EALREADYSTARTED = BRYPT_SERVICE_VALUE(2);
BRYPT_CONSTANT brypt_result_t BRYPT_ENOTSTARTED = BRYPT_SERVICE_VALUE(3);

BRYPT_CONSTANT brypt_result_t BRYPT_EFILENOTFOUND = BRYPT_CONFIGURATION_VALUE(1);
BRYPT_CONSTANT brypt_result_t BRYPT_EFILENOTSUPPORTED = BRYPT_CONFIGURATION_VALUE(2);
BRYPT_CONSTANT brypt_result_t BRYPT_EINVALIDCONFIG = BRYPT_CONFIGURATION_VALUE(127);

BRYPT_CONSTANT brypt_result_t BRYPT_EBINDINGFAILED = BRYPT_NETWORK_VALUE(1);
BRYPT_CONSTANT brypt_result_t BRYPT_ECONNECTIONFAILED = BRYPT_NETWORK_VALUE(2);
BRYPT_CONSTANT brypt_result_t BRYPT_EINVALIDADDRESS = BRYPT_NETWORK_VALUE(3);
BRYPT_CONSTANT brypt_result_t BRYPT_EADDRESSINUSE = BRYPT_NETWORK_VALUE(4);
BRYPT_CONSTANT brypt_result_t BRYPT_ENOTCONNECTED = BRYPT_NETWORK_VALUE(5);
BRYPT_CONSTANT brypt_result_t BRYPT_EALREADYCONNECTED = BRYPT_NETWORK_VALUE(6);
BRYPT_CONSTANT brypt_result_t BRYPT_ECONNECTIONREFUSED = BRYPT_NETWORK_VALUE(7);
BRYPT_CONSTANT brypt_result_t BRYPT_ENETWORKDOWN = BRYPT_NETWORK_VALUE(8);
BRYPT_CONSTANT brypt_result_t BRYPT_ENETWORKUNREACHABLE = BRYPT_NETWORK_VALUE(9);
BRYPT_CONSTANT brypt_result_t BRYPT_ENETWORKRESET = BRYPT_NETWORK_VALUE(10);
BRYPT_CONSTANT brypt_result_t BRYPT_ENETWORKPERMISSIONS = BRYPT_NETWORK_VALUE(11);
BRYPT_CONSTANT brypt_result_t BRYPT_ESESSIONCLOSED = BRYPT_NETWORK_VALUE(12);

//----------------------------------------------------------------------------------------------------------------------

typedef int32_t brypt_status_code_t;

BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_OK = 200;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_CREATED = 201;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_ACCEPTED = 202;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_NO_CONTENT = 204;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_PARTIAL_CONTENT = 206;

BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_MOVED_PERMANENTLY = 301;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_FOUND = 302;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_NOT_MODIFIED = 304;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_TEMPORARY_REDIRECT = 307;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_PERMANENT_REDIRECT = 308;

BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_BAD_REQUEST = 400;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_UNAUTHORIZED = 401;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_FORBIDDEN = 403;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_NOT_FOUND = 404;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_REQUEST_TIMEOUT = 408;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_CONFLICT = 409;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_PAYLOAD_TOO_LARGE = 413;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_URI_TOO_LONG = 414;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_IM_A_TEAPOT = 418;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_LOCKED = 423;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_UPGRADE_REQUIRED = 426;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_TOO_MANY_REQUESTS = 429;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS = 451;

BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_INTERNAL_SERVER_ERROR = 500;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_NOT_IMPLEMENTED = 501;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_SERVICE_UNAVAILABLE = 503;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_INSUFFICIENT_STORAGE = 507;
BRYPT_CONSTANT brypt_status_code_t BRYPT_STATUS_LOOP_DETECTED = 508;

//----------------------------------------------------------------------------------------------------------------------

BRYPT_CONSTANT size_t BRYPT_IDENTIFIER_MIN_SIZE = 31;
BRYPT_CONSTANT size_t BRYPT_IDENTIFIER_MAX_SIZE = 33;

//----------------------------------------------------------------------------------------------------------------------

BRYPT_CONSTANT int32_t BRYPT_UNKNOWN = -1;
BRYPT_CONSTANT int32_t BRYPT_DISABLE_CORE_THREAD = 0;

//----------------------------------------------------------------------------------------------------------------------

typedef int32_t brypt_identifier_type_t;
BRYPT_CONSTANT brypt_identifier_type_t BRYPT_IDENTIFIER_EPHEMERAL = 0;
BRYPT_CONSTANT brypt_identifier_type_t BRYPT_IDENTIFIER_PERSISTENT = 1;

//----------------------------------------------------------------------------------------------------------------------

typedef int32_t brypt_protocol_t;
BRYPT_CONSTANT brypt_protocol_t BRYPT_PROTOCOL_TCP = 1;

//----------------------------------------------------------------------------------------------------------------------

typedef int32_t brypt_connection_state_t;
BRYPT_CONSTANT brypt_connection_state_t BRYPT_CONNECTED_STATE = 1;
BRYPT_CONSTANT brypt_connection_state_t BRYPT_DISCONNECTED_STATE = 2;
BRYPT_CONSTANT brypt_connection_state_t BRYPT_RESOLVING_STATE = 3;

//----------------------------------------------------------------------------------------------------------------------

typedef int32_t brypt_authorization_state_t;
BRYPT_CONSTANT brypt_authorization_state_t BRYPT_UNAUTHORIZED_STATE = 1;
BRYPT_CONSTANT brypt_authorization_state_t BRYPT_AUTHORIZED_STATE = 2;
BRYPT_CONSTANT brypt_authorization_state_t BRYPT_FLAGGED_STATE = 3;

//----------------------------------------------------------------------------------------------------------------------

typedef int32_t brypt_strategy_t;
BRYPT_CONSTANT brypt_strategy_t BRYPT_STRATEGY_PQNISTL3 = 4099;

//----------------------------------------------------------------------------------------------------------------------

typedef int32_t brypt_log_level_t;
BRYPT_CONSTANT brypt_log_level_t BRYPT_LOG_LEVEL_OFF = 0;
BRYPT_CONSTANT brypt_log_level_t BRYPT_LOG_LEVEL_TRACE = 1;
BRYPT_CONSTANT brypt_log_level_t BRYPT_LOG_LEVEL_DEBUG = 2;
BRYPT_CONSTANT brypt_log_level_t BRYPT_LOG_LEVEL_INFO = 3;
BRYPT_CONSTANT brypt_log_level_t BRYPT_LOG_LEVEL_WARNING = 4;
BRYPT_CONSTANT brypt_log_level_t BRYPT_LOG_LEVEL_ERROR = 5;
BRYPT_CONSTANT brypt_log_level_t BRYPT_LOG_LEVEL_CRITICAL = 6;

//----------------------------------------------------------------------------------------------------------------------

typedef int32_t brypt_destination_type_t;
BRYPT_CONSTANT brypt_destination_type_t BRYPT_DESTINATION_CLUSTER = 2;
BRYPT_CONSTANT brypt_destination_type_t BRYPT_DESTINATION_NETWORK = 3;

//----------------------------------------------------------------------------------------------------------------------

BRYPT_CONSTANT int32_t BRYPT_REQUEST_KEY_SIZE = 16;

//----------------------------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif
//----------------------------------------------------------------------------------------------------------------------

BRYPT_DECLARE_HANDLE(service);
BRYPT_DECLARE_HANDLE(next_key);

//----------------------------------------------------------------------------------------------------------------------

struct brypt_option_endpoint_t
{
    brypt_protocol_t protocol;
    char const* interface;
    char const* binding;
    char const* bootstrap;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_remote_address_t
{
    brypt_protocol_t protocol;
    char const* uri;
    size_t size;
    bool bootstrapable;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_peer_statistics_t
{
    uint32_t sent;
    uint32_t received;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_peer_details_t
{
    brypt_connection_state_t connection_state;
    brypt_authorization_state_t authorization_state;
    brypt_remote_address_t const* remotes;
    size_t remotes_size;
    brypt_peer_statistics_t statistics;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_request_key_t 
{
    uint64_t high;
    uint64_t low;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_dispatch_t 
{
    size_t dispatched;
    brypt_result_t result;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_request_t 
{
    brypt_request_key_t key;
    size_t requested;
    brypt_result_t result;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_response_t
{
    brypt_request_key_t key;
    char const* source;
    uint8_t const* payload;
    size_t payload_size;
    brypt_status_code_t status_code;
    size_t remaining;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_next_respond_t 
{
    uint8_t const* payload;
    size_t payload_size;
    brypt_status_code_t status_code;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_next_dispatch_t 
{
    char const* route;
    uint8_t const* payload;
    size_t payload_size;
};

//----------------------------------------------------------------------------------------------------------------------

struct brypt_next_defer_t 
{
    struct notice_t 
    {
        brypt_destination_type_t type;
        char const* route;
        uint8_t const* payload;
        size_t payload_size;
    };

    struct response_t
    {
        uint8_t const* payload;
        size_t payload_size;
    };

    notice_t notice;
    response_t response;
};

//----------------------------------------------------------------------------------------------------------------------

typedef bool (*brypt_option_endpoint_reader_t) (brypt_option_endpoint_t const* const options, void* context);

//----------------------------------------------------------------------------------------------------------------------

typedef bool (*brypt_peer_statistics_reader_t) (brypt_peer_statistics_t const* const statistics, void* context);
typedef bool (*brypt_peer_details_reader_t) (brypt_peer_details_t const* const details, void* context);

//----------------------------------------------------------------------------------------------------------------------

typedef bool (*brypt_on_message_t) (
    char const* source, uint8_t const* payload, size_t size, brypt_next_key_t* next, void* context);

//----------------------------------------------------------------------------------------------------------------------

typedef void (*brypt_on_response_t) (brypt_response_t const* const response, void* context);
typedef void (*brypt_on_request_error_t) (brypt_response_t const* const response, void* context);

//----------------------------------------------------------------------------------------------------------------------

#define BRYPT_DECLARE_EVENT_CALLBACK(event_type, ...)\
    typedef void (*brypt_event_##event_type##_t) (__VA_ARGS__ __VA_OPT__(,) void* context);

BRYPT_DECLARE_EVENT_CALLBACK(
    binding_failed,
    brypt_protocol_t protocol, char const* uri, brypt_result_t result)

BRYPT_DECLARE_EVENT_CALLBACK(
    connection_failed,
    brypt_protocol_t protocol, char const* uri, brypt_result_t result)

BRYPT_DECLARE_EVENT_CALLBACK(
    endpoint_started,
    brypt_protocol_t protocol, char const* uri)

BRYPT_DECLARE_EVENT_CALLBACK(
    endpoint_stopped,
    brypt_protocol_t protocol, char const* uri, brypt_result_t result)

BRYPT_DECLARE_EVENT_CALLBACK(
    peer_connected, 
    char const* identifier, brypt_protocol_t protocol)

BRYPT_DECLARE_EVENT_CALLBACK(
    peer_connected, 
    char const* identifier, brypt_protocol_t protocol)

BRYPT_DECLARE_EVENT_CALLBACK(
    peer_disconnected, 
    char const* identifier, brypt_protocol_t protocol, brypt_result_t result)

BRYPT_DECLARE_EVENT_CALLBACK(
    runtime_started)

BRYPT_DECLARE_EVENT_CALLBACK(
    runtime_stopped,
    brypt_result_t result)

//----------------------------------------------------------------------------------------------------------------------

typedef void (*brypt_on_log_t) (brypt_log_level_t level, char const* message, size_t size, void* context);

//----------------------------------------------------------------------------------------------------------------------

BRYPT_EXPORT brypt_service_t* brypt_service_create() BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_service_start(brypt_service_t* const service) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_service_stop(brypt_service_t* const service) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_service_restart(brypt_service_t* const service) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_service_destroy(brypt_service_t* service) BRYPT_NOEXCEPT;

BRYPT_EXPORT bool brypt_option_get_bool(
    brypt_service_t const* const service, brypt_option_t option) BRYPT_NOEXCEPT;
BRYPT_EXPORT int32_t brypt_option_get_int32(
    brypt_service_t const* const service, brypt_option_t option) BRYPT_NOEXCEPT;
BRYPT_EXPORT char const* brypt_option_get_string(
    brypt_service_t const* const service, brypt_option_t option) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_result_t brypt_option_set_bool(
    brypt_service_t* const service, brypt_option_t option, bool value) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_option_set_int32(
    brypt_service_t* const service, brypt_option_t option, int32_t value) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_option_set_string(
    brypt_service_t* const service, brypt_option_t option, char const* value, size_t size) BRYPT_NOEXCEPT;

BRYPT_EXPORT bool brypt_option_is_bool_type(brypt_option_t option) BRYPT_NOEXCEPT;
BRYPT_EXPORT bool brypt_option_is_int_type(brypt_option_t option) BRYPT_NOEXCEPT;
BRYPT_EXPORT bool brypt_option_is_string_type(brypt_option_t option) BRYPT_NOEXCEPT;

BRYPT_EXPORT size_t brypt_option_get_endpoint_count(brypt_service_t const* const service) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_option_read_endpoints(
    brypt_service_t const* const service, brypt_option_endpoint_reader_t callback, void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_option_find_endpoint(
    brypt_service_t const* const service,
    brypt_protocol_t protocol,
    char const* binding,
    brypt_option_endpoint_reader_t callback,
    void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_option_attach_endpoint(
    brypt_service_t* const service, brypt_option_endpoint_t const* const options) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_option_detach_endpoint(
    brypt_service_t* const service, brypt_protocol_t protocol, char const* binding) BRYPT_NOEXCEPT;

BRYPT_EXPORT bool brypt_option_configuration_file_exists(brypt_service_t* const service) BRYPT_NOEXCEPT;
BRYPT_EXPORT bool brypt_option_configuration_validated(brypt_service_t* const service) BRYPT_NOEXCEPT;

BRYPT_EXPORT bool brypt_service_is_active(brypt_service_t const* const service) BRYPT_NOEXCEPT;

BRYPT_EXPORT size_t brypt_service_get_identifier(
    brypt_service_t* const service, char* buffer, size_t size) BRYPT_NOEXCEPT;

BRYPT_EXPORT bool brypt_service_is_peer_connected(
    brypt_service_t const* const service, char const* identifier) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_result_t brypt_service_get_peer_statistics(
    brypt_service_t const* const service,
    char const* identifier,
    brypt_peer_statistics_reader_t callback,
    void* context) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_result_t brypt_service_get_peer_details(
    brypt_service_t const* const service,
    char const* identifier,
    brypt_peer_details_reader_t callback,
    void* context) BRYPT_NOEXCEPT;

BRYPT_EXPORT size_t brypt_service_active_peer_count(brypt_service_t const* const service) BRYPT_NOEXCEPT;
BRYPT_EXPORT size_t brypt_service_inactive_peer_count(brypt_service_t const* const service) BRYPT_NOEXCEPT;
BRYPT_EXPORT size_t brypt_service_observed_peer_count(brypt_service_t const* const service) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_result_t brypt_service_register_route(
    brypt_service_t* const service, char const* route, brypt_on_message_t callback, void* context) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_result_t brypt_service_connect(
    brypt_service_t const* const service, brypt_protocol_t protocol, char const* address) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_service_disconnect_by_identifier(
    brypt_service_t const* const service, char const* identifier) BRYPT_NOEXCEPT;
BRYPT_EXPORT size_t brypt_service_disconnect_by_address(
    brypt_service_t const* const service, brypt_protocol_t protocol, char const* address) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_dispatch_t brypt_service_dispatch(
    brypt_service_t const* const service,
    char const* identifier,
    char const* route,
    uint8_t const* payload,
    size_t size) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_dispatch_t brypt_service_dispatch_cluster(
    brypt_service_t const* const service,
    char const* route,
    uint8_t const* payload,
    size_t size) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_dispatch_t brypt_service_dispatch_cluster_sample(
    brypt_service_t const* const service,
    char const* route,
    uint8_t const* payload,
    size_t size,
    double sample) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_request_t brypt_service_request(
    brypt_service_t const* const service,
    char const* identifier,
    char const* route,
    uint8_t const* payload,
    size_t size,
    brypt_on_response_t response_callback,
    brypt_on_request_error_t error_callback,
    void* context) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_request_t brypt_service_request_cluster(
    brypt_service_t const* const service,
    char const* route,
    uint8_t const* payload,
    size_t size,
    brypt_on_response_t response_callback,
    brypt_on_request_error_t error_callback,
    void* context) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_request_t brypt_service_request_cluster_sample(
    brypt_service_t const* const service,
    char const* route,
    uint8_t const* payload,
    size_t size,
    double sample,
    brypt_on_response_t response_callback,
    brypt_on_request_error_t error_callback,
    void* context) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_result_t brypt_next_respond(
    brypt_next_key_t const* const next, brypt_next_respond_t const* const options) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_next_dispatch(
    brypt_next_key_t const* const next, brypt_next_dispatch_t const* const options) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_next_defer(
    brypt_next_key_t const* const next, brypt_next_defer_t const* const options) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_result_t brypt_event_subscribe_binding_failed(
    brypt_service_t* const service, brypt_event_binding_failed_t callback, void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_event_subscribe_connection_failed(
    brypt_service_t* const service, brypt_event_connection_failed_t callback, void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_event_subscribe_endpoint_started(
    brypt_service_t* const service, brypt_event_endpoint_started_t callback, void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_event_subscribe_endpoint_stopped(
    brypt_service_t* const service, brypt_event_endpoint_stopped_t callback, void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_event_subscribe_peer_connected(
    brypt_service_t* const service, brypt_event_peer_connected_t callback, void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_event_subscribe_peer_disconnected(
    brypt_service_t* const service, brypt_event_peer_disconnected_t callback, void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_event_subscribe_runtime_started(
    brypt_service_t* const service, brypt_event_runtime_started_t callback, void* context) BRYPT_NOEXCEPT;
BRYPT_EXPORT brypt_result_t brypt_event_subscribe_runtime_stopped(
    brypt_service_t* const service, brypt_event_runtime_stopped_t callback, void* context) BRYPT_NOEXCEPT;

BRYPT_EXPORT brypt_result_t brypt_service_register_logger(
    brypt_service_t* const service, brypt_on_log_t callback, void* context) BRYPT_NOEXCEPT;

BRYPT_EXPORT char const* brypt_error_description(brypt_result_t code) BRYPT_NOEXCEPT;
BRYPT_EXPORT char const* brypt_status_code_description(brypt_status_code_t code) BRYPT_NOEXCEPT;

//----------------------------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
} // extern "C"
#endif
//----------------------------------------------------------------------------------------------------------------------
