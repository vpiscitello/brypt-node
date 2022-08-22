//----------------------------------------------------------------------------------------------------------------------
// Description: C++20 language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/protocol.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <compare>
#include <cstdint>
#include <string>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

class remote_address;
class peer_statistics;
class peer_details;

enum class connection_state : std::int32_t {
    unknown = BRYPT_UNKNOWN,
    connected = BRYPT_CONNECTED_STATE,
    disconnected = BRYPT_DISCONNECTED_STATE,
    resolving = BRYPT_RESOLVING_STATE,
};

enum class authorization_state : std::int32_t {
    unknown = BRYPT_UNKNOWN,
    unauthorized = BRYPT_UNAUTHORIZED_STATE,
    authorized = BRYPT_AUTHORIZED_STATE,
    flagged = BRYPT_FLAGGED_STATE
};

//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::remote_address {
//----------------------------------------------------------------------------------------------------------------------

class brypt::remote_address
{
public:
    remote_address() noexcept;
    remote_address(brypt_remote_address_t const& other);
    ~remote_address() = default;

    remote_address(remote_address const& other) = default;
    remote_address(remote_address&& other) noexcept = default;
    remote_address& operator=(remote_address const& other) = default;
    remote_address& operator=(remote_address&& other) noexcept = default;

    bool operator==(remote_address const& other) const noexcept;
    std::strong_ordering operator<=>(remote_address const& other) const noexcept;

    [[nodiscard]] protocol const& get_protocol() const;
    [[nodiscard]] std::string const& get_uri() const;
    [[nodiscard]] bool is_bootstrapable() const;

private:
    protocol m_protocol;
    std::string m_uri;
    bool m_bootstrapable;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::remote_address::remote_address() noexcept
    : m_protocol(protocol::unknown)
    , m_uri()
    , m_bootstrapable(false)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::remote_address::remote_address(brypt_remote_address_t const& other)
    : m_protocol(static_cast<protocol>(other.protocol))
    , m_uri(other.uri)
    , m_bootstrapable(other.bootstrapable)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::remote_address::operator==(remote_address const& other) const noexcept
{
    return (m_protocol == other.m_protocol) && (m_uri == other.m_uri);
}

//----------------------------------------------------------------------------------------------------------------------

inline std::strong_ordering brypt::remote_address::operator<=>(remote_address const& other) const noexcept
{
    if (auto const result = m_protocol <=> other.m_protocol; result != std::strong_ordering::equal) {
        return result;
    }
    return m_uri <=> other.m_uri;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::protocol const& brypt::remote_address::get_protocol() const { return m_protocol; }

//----------------------------------------------------------------------------------------------------------------------

inline std::string const& brypt::remote_address::get_uri() const { return m_uri; }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::remote_address::is_bootstrapable() const { return m_bootstrapable; }

//----------------------------------------------------------------------------------------------------------------------
// } brypt::remote_address
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::peer_statistics {
//----------------------------------------------------------------------------------------------------------------------

class brypt::peer_statistics
{
public:
    peer_statistics() noexcept;
    peer_statistics(brypt_peer_statistics_t const& other);
    ~peer_statistics() = default;

    peer_statistics(peer_statistics const& other) = default;
    peer_statistics(peer_statistics&& other) noexcept = default;
    peer_statistics& operator=(peer_statistics const& other) = default;
    peer_statistics& operator=(peer_statistics&& other) noexcept = default;

    std::uint32_t get_sent() const;
    std::uint32_t get_received() const;

private:
    std::uint32_t m_sent;
    std::uint32_t m_received;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::peer_statistics::peer_statistics() noexcept
    : m_sent(0)
    , m_received(0)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::peer_statistics::peer_statistics(brypt_peer_statistics_t const& other)
    : m_sent(other.sent)
    , m_received(other.received)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline std::uint32_t brypt::peer_statistics::get_sent() const { return m_sent; }

//----------------------------------------------------------------------------------------------------------------------

inline std::uint32_t brypt::peer_statistics::get_received() const { return m_received; }

//----------------------------------------------------------------------------------------------------------------------
// } brypt::peer_statistics
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::peer_details {
//----------------------------------------------------------------------------------------------------------------------

class brypt::peer_details
{
public:
    peer_details() noexcept;
    peer_details(brypt_peer_details_t const& other);
    ~peer_details() = default;

    peer_details(peer_details const& other) = default;
    peer_details(peer_details&& other) noexcept = default;
    peer_details& operator=(peer_details const& other) = default;
    peer_details& operator=(peer_details&& other) noexcept = default;

    connection_state get_connection_state() const;
    authorization_state get_authorization_state() const;
    std::vector<remote_address> const& get_remotes() const;
    std::uint32_t get_sent() const;
    std::uint32_t get_received() const;

private:
    connection_state m_connection;
    authorization_state m_authorization;
    std::vector<remote_address> m_remotes;
    peer_statistics m_statistics;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::peer_details::peer_details() noexcept
    : m_connection(connection_state::unknown)
    , m_authorization(authorization_state::unknown)
    , m_remotes()
    , m_statistics()
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::peer_details::peer_details(brypt_peer_details_t const& other)
    : m_connection(static_cast<brypt::connection_state>(other.connection_state))
    , m_authorization(static_cast<brypt::authorization_state>(other.authorization_state))
    , m_remotes()
    , m_statistics(other.statistics)
{
    for (std::size_t idx = 0; idx < other.remotes_size; ++idx) {
        m_remotes.emplace_back(remote_address{ other.remotes[idx] });
    }
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::connection_state brypt::peer_details::get_connection_state() const { return m_connection; }

//----------------------------------------------------------------------------------------------------------------------

inline brypt::authorization_state brypt::peer_details::get_authorization_state() const { return m_authorization; }

//----------------------------------------------------------------------------------------------------------------------

inline std::vector<brypt::remote_address> const& brypt::peer_details::get_remotes() const { return m_remotes; }

//----------------------------------------------------------------------------------------------------------------------

inline std::uint32_t brypt::peer_details::get_sent() const { return m_statistics.get_sent(); }

//----------------------------------------------------------------------------------------------------------------------

inline std::uint32_t brypt::peer_details::get_received() const { return m_statistics.get_received(); }

//----------------------------------------------------------------------------------------------------------------------
// } brypt::peer_details
//----------------------------------------------------------------------------------------------------------------------
