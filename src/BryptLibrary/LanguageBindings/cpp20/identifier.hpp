//----------------------------------------------------------------------------------------------------------------------
// Description: C++20 language bindings for the Brypt Shared Library C Interface.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <brypt/brypt.h>
//----------------------------------------------------------------------------------------------------------------------
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace brypt {
//----------------------------------------------------------------------------------------------------------------------

class identifier;

enum class identifier_persistence : std::int32_t {
    unknown = BRYPT_UNKNOWN,
    ephemeral = BRYPT_IDENTIFIER_EPHEMERAL,
    persistent = BRYPT_IDENTIFIER_PERSISTENT
};

//----------------------------------------------------------------------------------------------------------------------
} // brypt namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// brypt::identifier {
//----------------------------------------------------------------------------------------------------------------------

class brypt::identifier
{
public:
    identifier() noexcept;
    explicit identifier(std::string_view external);
    ~identifier() = default;

    identifier(identifier const& other);
    identifier(identifier&& other) noexcept;
    identifier& operator=(identifier const& other);
    identifier& operator=(identifier&& other) noexcept;
    identifier& operator=(std::string_view external);
    identifier& operator=(std::string&& external);

    operator std::string const& () const;
    operator std::string& ();
    operator std::string_view() const;

    bool operator==(identifier const& other) const noexcept;
    bool operator==(std::string_view external) const noexcept;
    std::strong_ordering operator<=>(identifier const& other) const noexcept;
    std::strong_ordering operator<=>(std::string_view external) const noexcept;

    [[nodiscard]] std::string const& as_string() const noexcept;

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::string m_external;
};

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier::identifier() noexcept
    : m_external()
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier::identifier(std::string_view external)
    : m_external(external)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier::identifier(identifier const& other)
    : m_external(other.m_external)
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier::identifier(identifier&& other) noexcept
    : m_external(std::move(other.m_external))
{
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier& brypt::identifier::operator=(identifier const& other)
{
    m_external = other.m_external;
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier& brypt::identifier::operator=(identifier&& other) noexcept
{
    m_external = std::move(other.m_external);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier& brypt::identifier::operator=(std::string_view external)
{
    m_external = external;
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier& brypt::identifier::operator=(std::string&& external)
{
    m_external = std::move(external);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier::operator std::string const& () const { return m_external; }

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier::operator std::string& () { return m_external; }

//----------------------------------------------------------------------------------------------------------------------

inline brypt::identifier::operator std::string_view() const { return m_external; }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::identifier::operator==(identifier const& other) const noexcept
{
    return m_external == other.m_external;
}

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::identifier::operator==(std::string_view external) const noexcept
{
    return m_external == external;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::strong_ordering brypt::identifier::operator<=>(identifier const& other) const noexcept
{
    return m_external <=> other.m_external;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::strong_ordering brypt::identifier::operator<=>(std::string_view external) const noexcept
{
    return m_external <=> external;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::string const& brypt::identifier::as_string() const noexcept { return m_external; }

//----------------------------------------------------------------------------------------------------------------------

inline bool brypt::identifier::empty() const noexcept { return m_external.empty(); }

//----------------------------------------------------------------------------------------------------------------------

inline std::size_t brypt::identifier::size() const noexcept { return m_external.size(); }

//----------------------------------------------------------------------------------------------------------------------
// } brypt::identifier
//----------------------------------------------------------------------------------------------------------------------
