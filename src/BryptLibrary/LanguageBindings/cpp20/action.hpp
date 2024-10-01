//----------------------------------------------------------------------------------------------------------------------
// Description: C++20 language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/result.hpp>
#include <brypt/status.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <compare>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

class request_key;
class response;
class next;

enum class destination : std::int32_t {
    unknown = BRYPT_UNKNOWN,
    cluster = BRYPT_DESTINATION_CLUSTER,
    network = BRYPT_DESTINATION_NETWORK
};

using response_callback = std::function<void(response const&)>;
using request_error_callback = std::function<void(response const&)>;

//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::request_key {
//----------------------------------------------------------------------------------------------------------------------

class brypt::request_key
{
public:
    static constexpr std::size_t size = BRYPT_REQUEST_KEY_SIZE;

    request_key() noexcept = default;
    request_key(request_key const& other) noexcept = default;
    request_key(request_key&& other) noexcept = default;
    request_key(brypt_request_key_t other) noexcept;

    request_key& operator=(request_key const& other) noexcept = default;
    request_key& operator=(request_key&& other) noexcept = default;
    request_key& operator=(brypt_request_key_t&& other) noexcept;

    [[nodiscard]] operator bool() const noexcept;
    [[nodiscard]] bool operator==(request_key const& other) const noexcept;
    [[nodiscard]] std::strong_ordering operator<=>(request_key const& other) const noexcept;

private:
    std::int64_t m_high;
    std::uint64_t m_low;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::request_key::request_key(brypt_request_key_t other) noexcept
    : m_high(other.high)
    , m_low(other.low)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::request_key& brypt::request_key::operator=(brypt_request_key_t&& other) noexcept
{
    m_high = std::move(other.high);
    m_low = std::move(other.low);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::request_key::operator bool() const noexcept
{
    return m_high != 0 && m_low != 0;
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::request_key::operator==(request_key const& other) const noexcept
{
    return (m_high == other.m_high) && (m_low == other.m_low);
}

//----------------------------------------------------------------------------------------------------------------------

inline std::strong_ordering brypt::request_key::operator<=>(request_key const& other) const noexcept
{
    if (auto const result = m_high <=> other.m_high; result != std::strong_ordering::equal) {
        return result;
    }
    return m_low <=> other.m_low;
}

//----------------------------------------------------------------------------------------------------------------------
// } brypt::request_key
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// } brypt::response
//----------------------------------------------------------------------------------------------------------------------

class brypt::response
{
public:
    explicit response(brypt_response_t const& _response);
    ~response() = default;

    response(response const& other) = delete;
    response(response&& other) = delete;
    response& operator=(response const& other) = delete;
    response& operator=(response&& other) = delete;

    std::string_view const& get_source() const;
    std::span<std::uint8_t const> const& get_payload() const;
    status get_status() const;

private:
    std::string_view m_source;
    std::span<std::uint8_t const> m_payload;
    status m_status_code;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::response::response(brypt_response_t const& _response)
    : m_source(_response.source)
    , m_payload(_response.payload, _response.payload_size)
    , m_status_code(static_cast<status_code>(_response.status_code))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline std::string_view const& brypt::response::get_source() const { return m_source; }

//----------------------------------------------------------------------------------------------------------------------

inline std::span<std::uint8_t const> const& brypt::response::get_payload() const { return m_payload; }

//----------------------------------------------------------------------------------------------------------------------

inline brypt::status brypt::response::get_status() const { return m_status_code; }

//----------------------------------------------------------------------------------------------------------------------
// } brypt::response
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::next {
//----------------------------------------------------------------------------------------------------------------------

class brypt::next
{
public:
    struct deferred_options {
        struct Notice {
            destination type;
            std::string_view route;
            std::span<std::uint8_t const> payload;
        };

        struct Response {
            std::span<std::uint8_t const> payload;
        };

        Notice notice;
        Response response;
    };

    explicit next(brypt_next_key_t const* const key);
    ~next() = default;

    next(next const& other) = delete;
    next(next&& other) = delete;
    next& operator=(next const& other) = delete;
    next& operator=(next&& other) = delete;

    [[nodiscard]] result respond(status status) const;
    [[nodiscard]] result respond(std::span<std::uint8_t const> payload, status status) const;
    [[nodiscard]] result dispatch(std::string_view route, std::span<std::uint8_t const> payload) const;
    [[nodiscard]] result defer(deferred_options&& options);

private:
    brypt_next_key_t const* const m_key;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::next::next(brypt_next_key_t const* const key)
    : m_key(key)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::next::respond(status status) const
{
    return respond(std::span<std::uint8_t const>{}, status);
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::next::respond(std::span<std::uint8_t const> payload, status status) const
{
    auto const options = brypt_next_respond_t{
        .payload = payload.data(),
        .payload_size = payload.size(),
        .status_code = status.code()
    };
    return brypt_next_respond(m_key, &options);
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::next::dispatch(std::string_view route, std::span<std::uint8_t const> payload) const
{
    auto const options = brypt_next_dispatch_t{
        .route = route.data(),
        .payload = payload.data(),
        .payload_size = payload.size()
    };
    return brypt_next_dispatch(m_key, &options);
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::result brypt::next::defer(deferred_options&& options)
{
    auto const _options = brypt_next_defer_t{
        .notice = {
            .type = static_cast<brypt_destination_type_t>(options.notice.type),
            .route = options.notice.route.data(),
            .payload = options.notice.payload.data(),
            .payload_size = options.notice.payload.size()
        },
        .response = {
            .payload = options.response.payload.data(),
            .payload_size = options.response.payload.size()
        }
    };
    return brypt_next_defer(m_key, &_options);
}

//----------------------------------------------------------------------------------------------------------------------
// } brypt::next
//----------------------------------------------------------------------------------------------------------------------
