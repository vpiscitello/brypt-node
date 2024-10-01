//----------------------------------------------------------------------------------------------------------------------
// Description: C++20 language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <compare>
#include <cstdint>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

class status;

enum class status_code : brypt_status_code_t {
    unknown = BRYPT_UNKNOWN,
    ok = BRYPT_STATUS_OK,
    created = BRYPT_STATUS_CREATED,
    accepted = BRYPT_STATUS_ACCEPTED,
    no_content = BRYPT_STATUS_NO_CONTENT,
    partial_content = BRYPT_STATUS_PARTIAL_CONTENT,
    moved_permanently = BRYPT_STATUS_MOVED_PERMANENTLY,
    found = BRYPT_STATUS_FOUND,
    not_modified = BRYPT_STATUS_NOT_MODIFIED,
    temporary_redirect = BRYPT_STATUS_TEMPORARY_REDIRECT,
    permanent_redirect = BRYPT_STATUS_PERMANENT_REDIRECT,
    bad_request = BRYPT_STATUS_BAD_REQUEST,
    unauthorized = BRYPT_STATUS_UNAUTHORIZED,
    forbidden = BRYPT_STATUS_FORBIDDEN,
    not_found = BRYPT_STATUS_NOT_FOUND,
    request_timeout = BRYPT_STATUS_REQUEST_TIMEOUT,
    conflict = BRYPT_STATUS_CONFLICT,
    payload_too_large = BRYPT_STATUS_PAYLOAD_TOO_LARGE,
    uri_too_long = BRYPT_STATUS_URI_TOO_LONG,
    im_a_teapot = BRYPT_STATUS_IM_A_TEAPOT,
    locked = BRYPT_STATUS_LOCKED,
    upgrade_required = BRYPT_STATUS_UPGRADE_REQUIRED,
    too_many_requests = BRYPT_STATUS_TOO_MANY_REQUESTS,
    unavailable_for_legal_reasons = BRYPT_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS,
    internal_server_error = BRYPT_STATUS_INTERNAL_SERVER_ERROR,
    not_implemented = BRYPT_STATUS_NOT_IMPLEMENTED,
    service_unavailable = BRYPT_STATUS_SERVICE_UNAVAILABLE,
    insufficient_storage = BRYPT_STATUS_INSUFFICIENT_STORAGE,
    loop_detected = BRYPT_STATUS_LOOP_DETECTED,
};

//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::status {
//----------------------------------------------------------------------------------------------------------------------

class brypt::status
{
public:
    status() noexcept;
    status(brypt_status_code_t code) noexcept;
    status(status_code code) noexcept;

    ~status() = default;
    status(status const& other) = default;
    status(status&& other) = default;
    status& operator=(status const& other) = default;
    status& operator=(status&& other) = default;

    bool operator==(status const& other) const noexcept;
    bool operator==(status_code other) const noexcept;
    bool operator!=(status const& other) const noexcept;
    bool operator!=(status_code other) const noexcept;

    operator bool() const noexcept;

    [[nodiscard]] char const* message() const noexcept;
    [[nodiscard]] bool has_success_code() const noexcept;
    [[nodiscard]] bool has_error_code() const noexcept;
    [[nodiscard]] brypt_status_code_t code() const noexcept;

private:
    brypt_status_code_t m_code;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::status::status() noexcept
    : m_code(static_cast<brypt_status_code_t>(status_code::unknown))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::status::status(brypt_status_code_t code) noexcept
    : m_code(code)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::status::status(status_code code) noexcept
    : m_code(static_cast<brypt_status_code_t>(code))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::status::operator==(status const& other) const noexcept { return m_code == other.m_code; }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::status::operator==(status_code other) const noexcept
{
    return m_code == static_cast<brypt_status_code_t>(other);
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::status::operator!=(status const& other) const noexcept { return !operator==(other); }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::status::operator!=(status_code other) const noexcept
{
    return m_code != static_cast<brypt_status_code_t>(other);
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::status::operator bool() const noexcept { return has_success_code(); }

//----------------------------------------------------------------------------------------------------------------------

inline char const* brypt::status::message() const noexcept { return brypt_status_code_description(m_code); }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::status::has_success_code() const noexcept { return m_code < 300; }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::status::has_error_code() const noexcept { return !has_success_code(); }

//----------------------------------------------------------------------------------------------------------------------

inline brypt_status_code_t brypt::status::code() const noexcept { return m_code; }

//----------------------------------------------------------------------------------------------------------------------
// } brypt::status
//----------------------------------------------------------------------------------------------------------------------
