//----------------------------------------------------------------------------------------------------------------------
// Description: The Brypt Shared Library. This library will allow external applications manage
// the brypt binary 
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

#if defined(__cplusplus) && !defined(_Static_assert)
    #define _Static_assert static_assert
#endif
#define BRYPT_STATIC_ASSERT(test, reason) _Static_assert((test), #reason)

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

BRYPT_CONSTANT brypt_option_t BRYPT_OPT_USE_BOOTSTRAPS = BRYPT_GENERIC_VALUE(10);

BRYPT_CONSTANT brypt_option_t BRYPT_OPT_BASE_FILEPATH = BRYPT_CONFIGURATION_VALUE(1);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_CONFIGURATION_FILENAME = BRYPT_CONFIGURATION_VALUE(2);
BRYPT_CONSTANT brypt_option_t BRYPT_OPT_PEERS_FILENAME = BRYPT_CONFIGURATION_VALUE(3);

//----------------------------------------------------------------------------------------------------------------------

typedef uint32_t brypt_status_t;

BRYPT_CONSTANT brypt_status_t BRYPT_ACCEPTED = 0;

BRYPT_CONSTANT brypt_status_t BRYPT_EUNSPECIFIED = BRYPT_GENERIC_VALUE(1);
BRYPT_CONSTANT brypt_status_t BRYPT_EACCESSDENIED = BRYPT_GENERIC_VALUE(2);
BRYPT_CONSTANT brypt_status_t BRYPT_EINVALIDARGUMENT = BRYPT_GENERIC_VALUE(3);
BRYPT_CONSTANT brypt_status_t BRYPT_EOPERNOTSUPPORTED = BRYPT_GENERIC_VALUE(4);
BRYPT_CONSTANT brypt_status_t BRYPT_EOPERTIMEOUT = BRYPT_GENERIC_VALUE(5);

BRYPT_CONSTANT brypt_status_t BRYPT_EINITNFAILURE = BRYPT_SERVICE_VALUE(0);
BRYPT_CONSTANT brypt_status_t BRYPT_EALREADYSTARTED = BRYPT_SERVICE_VALUE(1);
BRYPT_CONSTANT brypt_status_t BRYPT_ENOTSTARTED = BRYPT_SERVICE_VALUE(2);
BRYPT_CONSTANT brypt_status_t BRYPT_ESHUTDOWNFAILURE = BRYPT_SERVICE_VALUE(3);

BRYPT_CONSTANT brypt_status_t BRYPT_EFILENOTFOUND = BRYPT_CONFIGURATION_VALUE(0);
BRYPT_CONSTANT brypt_status_t BRYPT_EFILENOTSUPPORTED = BRYPT_CONFIGURATION_VALUE(1);
BRYPT_CONSTANT brypt_status_t BRYPT_EMISSINGPARAMETER = BRYPT_CONFIGURATION_VALUE(2);

BRYPT_CONSTANT brypt_status_t BRYPT_ENETBINDFAILED = BRYPT_NETWORK_VALUE(0);
BRYPT_CONSTANT brypt_status_t BRYPT_ENETCONNNFAILED = BRYPT_NETWORK_VALUE(1);

//----------------------------------------------------------------------------------------------------------------------

BRYPT_CONSTANT size_t BRYPT_IDENTIFIER_MIN_SIZE = 31;
BRYPT_CONSTANT size_t BRYPT_IDENTIFIER_MAX_SIZE = 33;

//----------------------------------------------------------------------------------------------------------------------

#define DECLARE_HANDLE(name) struct name; typedef struct name##_t name##_t

//----------------------------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
#endif
//----------------------------------------------------------------------------------------------------------------------

DECLARE_HANDLE(brypt_service);

BRYPT_EXPORT brypt_service_t* brypt_service_create(char const* base_path, size_t base_path_size);
BRYPT_EXPORT brypt_status_t brypt_service_initialize(brypt_service_t* const service);
BRYPT_EXPORT brypt_status_t brypt_service_start(brypt_service_t* const service);
BRYPT_EXPORT brypt_status_t brypt_service_stop(brypt_service_t* const service);
BRYPT_EXPORT brypt_status_t brypt_service_destroy(brypt_service_t* service);

BRYPT_EXPORT brypt_status_t brypt_option_set_int(
    brypt_service_t* const service, brypt_option_t option, int32_t value);
BRYPT_EXPORT brypt_status_t brypt_option_set_bool(
    brypt_service_t* const service, brypt_option_t option, bool value);
BRYPT_EXPORT brypt_status_t brypt_option_set_str(
    brypt_service_t* const service, brypt_option_t option, char const* value, size_t size);

BRYPT_EXPORT int32_t brypt_option_get_int(brypt_service_t const* const service, brypt_option_t option);
BRYPT_EXPORT bool brypt_option_get_bool(brypt_service_t const* const service, brypt_option_t option);
BRYPT_EXPORT char const* brypt_option_get_str(brypt_service_t const* const service, brypt_option_t option);

BRYPT_EXPORT bool brypt_service_is_active(brypt_service_t const* const service);

BRYPT_EXPORT size_t brypt_service_get_identifier(brypt_service_t const* const service, char* buffer, size_t size);

BRYPT_EXPORT size_t brypt_service_active_peer_count(brypt_service_t const* const service);
BRYPT_EXPORT size_t brypt_service_inactive_peer_count(brypt_service_t const* const service);
BRYPT_EXPORT size_t brypt_service_observed_peer_count(brypt_service_t const* const service);

BRYPT_EXPORT char const* brypt_error_description(brypt_status_t code);

//----------------------------------------------------------------------------------------------------------------------
#if defined(__cplusplus)
} // extern "C"
#endif
//----------------------------------------------------------------------------------------------------------------------
