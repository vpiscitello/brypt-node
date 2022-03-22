//----------------------------------------------------------------------------------------------------------------------
// File: Address.hpp
// Description: A class to encapsulate connection uris (i.e. tcp://127.0.0.1:35216). 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "Protocol.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <spdlog/fmt/bundled/format.h>
//----------------------------------------------------------------------------------------------------------------------
#include <compare>
#include <concepts>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Network {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view Wildcard = "*"; 
constexpr std::string_view ComponentSeperator = ":";
constexpr std::string_view SchemeSeperator = "://";

class Address;
class BindingAddress;
class RemoteAddress;

template<typename AddressType>
concept ValidAddressType = std::derived_from<AddressType, Network::Address>;

template <ValidAddressType AddressType>
struct AddressHasher;

//----------------------------------------------------------------------------------------------------------------------
namespace Socket {
//----------------------------------------------------------------------------------------------------------------------

enum class Type : std::uint8_t { IPv4, IPv6, Invalid };

struct Components;

[[nodiscard]] Type ParseAddressType(Address const& address);
[[nodiscard]] Type ParseAddressType(std::string_view const& partition);
[[nodiscard]] bool IsValidAddressSize(Address const& address);
[[nodiscard]] bool IsValidPortNumber(std::string_view const& partition);
[[nodiscard]] Components GetAddressComponents(Address const& address);

//----------------------------------------------------------------------------------------------------------------------
} // Socket namespace
} // Network namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::Address
{
public:
    virtual ~Address() = default;
    
    Address& operator=(Address const& other);
    Address(Address const& other);
    Address& operator=(Address&& other);
    Address(Address&& other);

    [[nodiscard]] std::strong_ordering operator<=>(Address const& other) const;
    [[nodiscard]] bool operator==(Address const& other) const;
    [[nodiscard]] bool operator!=(Address const& other) const;

    [[nodiscard]] Network::Protocol GetProtocol() const;
    [[nodiscard]] std::string const& GetUri() const;
    [[nodiscard]] std::string_view const& GetScheme() const;
    [[nodiscard]] std::string_view const& GetAuthority() const;

    [[nodiscard]] std::size_t GetSize() const;
    [[nodiscard]] bool IsValid() const;

    template <ValidAddressType AddressType>
    [[nodiscard]] bool Equivalent(AddressType const& other) const;

    // Socket Address Helpers {
    friend Socket::Type Socket::ParseAddressType(Address const& address);
    friend bool Socket::IsValidAddressSize(Address const& address);
    friend Socket::Components Socket::GetAddressComponents(Address const& address);
    // } Socket Address Helpers

protected:
    Address();
    Address(Protocol protocol, std::string_view uri, bool bootstrapable);

    [[nodiscard]] bool CacheAddressPartitions();
    [[nodiscard]] std::size_t GetSchemeBoundary();
    [[nodiscard]] std::size_t PrependScheme();
    void Reset();

    Protocol m_protocol;
    std::string m_uri;
    std::string_view m_scheme;
    std::string_view m_authority;
    std::string_view m_primary;
    std::string_view m_secondary;

    bool m_bootstrapable;
};

//----------------------------------------------------------------------------------------------------------------------

template <Network::ValidAddressType AddressType>
bool Network::Address::Equivalent(AddressType const& other) const
{
    return operator==(static_cast<Address const&>(other));
}

//----------------------------------------------------------------------------------------------------------------------

template <>
inline bool Network::Address::Equivalent(Address const& other) const
{
    return operator==(other);
}

//----------------------------------------------------------------------------------------------------------------------

class Network::BindingAddress : public Network::Address
{
public:
    BindingAddress();
    BindingAddress(Protocol protocol, std::string_view uri, std::string_view interface);
    ~BindingAddress() = default;

    BindingAddress& operator=(BindingAddress const& other) = default;
    BindingAddress(BindingAddress const& other) = default;
    BindingAddress& operator=(BindingAddress&& other) = default;
    BindingAddress(BindingAddress&& other) = default;

    std::strong_ordering operator<=>(BindingAddress const& other) const;
    [[nodiscard]] bool operator==(BindingAddress const& other) const;
    [[nodiscard]] bool operator!=(BindingAddress const& other) const;

    [[nodiscard]] std::string const& GetInterface() const;

private:
    std::string m_interface;
};

//----------------------------------------------------------------------------------------------------------------------

class Network::RemoteAddress : public Network::Address
{
public:
    enum class Origin : std::uint32_t { Invalid, Cache, Network, User };

    RemoteAddress();
    RemoteAddress(Protocol protocol, std::string_view uri, bool bootstrapable, Origin origin = Origin::Network);
    ~RemoteAddress() = default;

    RemoteAddress& operator=(RemoteAddress const& other) = default;
    RemoteAddress(RemoteAddress const& other) = default;
    RemoteAddress& operator=(RemoteAddress&& other) = default;
    RemoteAddress(RemoteAddress&& other) = default;
    
    std::strong_ordering operator<=>(RemoteAddress const& other) const;
    [[nodiscard]] bool operator==(RemoteAddress const& other) const;
    [[nodiscard]] bool operator!=(RemoteAddress const& other) const;

    [[nodiscard]] bool IsBootstrapable() const;
    [[nodiscard]] Origin GetOrigin() const;

private:
    Origin m_origin;
};

//----------------------------------------------------------------------------------------------------------------------

template <Network::ValidAddressType AddressType>
struct Network::AddressHasher
{
    std::size_t operator()(AddressType const& address) const
    {
        return std::hash<std::string>()(address.GetUri());
    }
};

//----------------------------------------------------------------------------------------------------------------------

struct Network::Socket::Components
{
    Components(std::string_view const& ip, std::string_view const& port);

    [[nodiscard]] std::string_view const& GetIPAddress() const;
    [[nodiscard]] std::string_view const& GetPort() const;
    [[nodiscard]] std::uint16_t GetPortNumber() const;

    std::string_view const& ip;
    std::string_view const& port;
};

//----------------------------------------------------------------------------------------------------------------------

template <Network::ValidAddressType AddressType>
struct fmt::formatter<AddressType>
{
    constexpr auto parse(format_parse_context& ctx)
    {
        auto const begin = ctx.begin();
        if (begin != ctx.end() && *begin != '}') {  throw format_error("invalid format");}
        return begin;
    }

    template <typename FormatContext>
    auto format(Network::Address const& address, FormatContext& ctx)
    {
        if (!address.IsValid()) {
            return format_to(ctx.out(), "[Unknown Address]");
        }
        return format_to(ctx.out(), "{}", address.GetUri());
    }
};

//----------------------------------------------------------------------------------------------------------------------
