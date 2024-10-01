//----------------------------------------------------------------------------------------------------------------------
// Description: C++20 language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <stdexcept>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

class result;

enum class result_code : brypt_result_t {
    accepted = BRYPT_ACCEPTED,
    canceled = BRYPT_ECANCELED,
    shutdown_requested = BRYPT_ESHUTDOWNREQUESTED,
    invalid_argument = BRYPT_EINVALIDARGUMENT,
    bad_alloc = BRYPT_EACCESSDENIED,
    access_denied = BRYPT_EACCESSDENIED,
    timeout = BRYPT_ETIMEOUT,
    conflict = BRYPT_ECONFLICT,
    missing_field = BRYPT_EMISSINGFIELD,
    payload_too_large = BRYPT_EPAYLOADTOOLARGE,
    out_of_memory = BRYPT_EOUTOFMEMORY,
    not_available = BRYPT_ENOTAVAILABLE,
    not_supported = BRYPT_ENOTSUPPORTED,
    unspecified = BRYPT_EUNSPECIFIED,
    not_implemented = BRYPT_ENOTIMPLEMENTED,
    initialization_failure = BRYPT_EINITFAILURE,
    already_started = BRYPT_EALREADYSTARTED,
    not_started = BRYPT_ENOTSTARTED,
    file_not_found = BRYPT_EFILENOTFOUND,
    file_not_supported = BRYPT_EFILENOTSUPPORTED,
    invalid_configuration = BRYPT_EINVALIDCONFIG,
    binding_failed = BRYPT_EBINDINGFAILED,
    connection_failed = BRYPT_ECONNECTIONFAILED,
    invalid_address = BRYPT_EINVALIDADDRESS,
    address_in_use = BRYPT_EADDRESSINUSE,
    not_connected = BRYPT_ENOTCONNECTED,
    already_connected = BRYPT_EALREADYCONNECTED,
    connection_refused = BRYPT_ECONNECTIONREFUSED,
    network_down = BRYPT_ENETWORKDOWN,
    network_reset = BRYPT_ENETWORKRESET,
    network_unreachable = BRYPT_ENETWORKUNREACHABLE,
    session_closed = BRYPT_ESESSIONCLOSED,
};

//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::result {
//----------------------------------------------------------------------------------------------------------------------

class brypt::result : public std::exception
{
public:
    result() noexcept;
    result(brypt_result_t result) noexcept;
    result(result_code result) noexcept;

    ~result() = default;
    result(result const& other) = default;
    result(result&& other) = default;
    result& operator=(result const& other) = default;
    result& operator=(result&& other) = default;

    bool operator==(result const& other) const noexcept;
    bool operator==(result_code other) const noexcept;
    bool operator!=(result const& other) const noexcept;
    bool operator!=(result_code other) const noexcept;

    operator bool() const noexcept;

    [[nodiscard]] virtual char const* what() const noexcept override;
    [[nodiscard]] bool is_success() const noexcept;
    [[nodiscard]] bool is_error() const noexcept;
    [[nodiscard]] brypt_result_t value() const noexcept;

private:
    brypt_result_t m_result;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result::result() noexcept
    : std::exception()
    , m_result(static_cast<brypt_result_t>(result_code::accepted))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result::result(brypt_result_t result) noexcept
    : std::exception()
    , m_result(result)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result::result(result_code result) noexcept
    : std::exception()
    , m_result(static_cast<brypt_result_t>(result))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::result::operator==(result const& other) const noexcept { return m_result == other.m_result; }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::result::operator==(result_code other) const noexcept { return m_result == static_cast<brypt_result_t>(other); }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::result::operator!=(result const& other) const noexcept { return !operator==(other); }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::result::operator!=(result_code other) const noexcept { return m_result != static_cast<brypt_result_t>(other); }

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result::operator bool() const noexcept { return is_success(); }

//----------------------------------------------------------------------------------------------------------------------

inline char const* brypt::result::what() const noexcept { return brypt_error_description(m_result); }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::result::is_success() const noexcept
{
    // Note: BRYPT_ACCEPTED, BRYPT_ECANCELED, and BRYPT_ESHUTDOWNREQUESTED are not considered error conditions and 
    // do not indicate some error needs to be handled. They are either returned as an immediate result or explicit 
    // action by the user. 
    switch (m_result) {
    case BRYPT_ACCEPTED:
    case BRYPT_ECANCELED:
    case BRYPT_ESHUTDOWNREQUESTED: return true;
    default: return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::result::is_error() const noexcept { return !is_success(); }

//----------------------------------------------------------------------------------------------------------------------

inline brypt_result_t brypt::result::value() const noexcept { return m_result; }

//----------------------------------------------------------------------------------------------------------------------
// } brypt::result
//----------------------------------------------------------------------------------------------------------------------
