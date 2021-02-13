//------------------------------------------------------------------------------------------------
// File: Address.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "Address.hpp"
//------------------------------------------------------------------------------------------------
#include "LoRa/EndpointDefinitions.hpp"
#include "TCP/EndpointDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <boost/lexical_cast.hpp>
#include <boost/asio/ip/address.hpp>
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

std::string BuildBindingUri(
    Network::Protocol protocol, std::string_view uri, std::string_view interface);
std::string GetInterfaceAddress(std::string_view interface);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Network::Address {
//------------------------------------------------------------------------------------------------

Network::Address::Address()
    : m_protocol(Protocol::Invalid)
    , m_uri()
    , m_scheme()
    , m_authority()
    , m_primary()
    , m_secondary()
    , m_bootstrapable(false)
{
}

//------------------------------------------------------------------------------------------------

Network::Address::Address(Protocol protocol, std::string_view uri, bool bootstrapable)
    : m_protocol(protocol)
    , m_uri(uri)
    , m_scheme()
    , m_authority()
    , m_primary()
    , m_secondary()
    , m_bootstrapable(bootstrapable)
{
    if (!CacheAddressPartitions()) { Reset(); }
}

//------------------------------------------------------------------------------------------------

Network::Address& Network::Address::operator=(Address const& other)
{
    m_protocol = other.m_protocol;
    m_uri = other.m_uri;
    m_bootstrapable = other.m_bootstrapable;
    if (!CacheAddressPartitions()) { Reset(); }
    return *this;
}

//------------------------------------------------------------------------------------------------

Network::Address::Address(Address const& other)
    : m_protocol(other.m_protocol)
    , m_uri(other.m_uri)
    , m_scheme()
    , m_authority()
    , m_primary()
    , m_secondary()
    , m_bootstrapable(other.m_bootstrapable)
{
    if (!CacheAddressPartitions()) { Reset(); }
}

//------------------------------------------------------------------------------------------------

Network::Address::Address(Address&& other)
    : m_protocol(std::move(other.m_protocol))
    , m_uri(std::move(other.m_uri))
    , m_scheme(std::move(other.m_scheme))
    , m_authority(std::move(other.m_authority))
    , m_primary(std::move(other.m_primary))
    , m_secondary(std::move(other.m_secondary))
    , m_bootstrapable(std::move(other.m_bootstrapable))
{
    other.m_protocol = Protocol::Invalid;
    other.m_scheme = {};
    other.m_authority = {};
    other.m_primary = {};
    other.m_secondary = {};
    other.m_bootstrapable = false;
}

//------------------------------------------------------------------------------------------------


bool Network::Address::operator==(Address const& other) const
{
    bool const equal = (m_protocol == other.m_protocol) && (m_uri == other.m_uri);
    return equal;
}

//------------------------------------------------------------------------------------------------

bool Network::Address::operator!=(Address const& other) const
{
    return !operator==(other);
}

//------------------------------------------------------------------------------------------------

Network::Protocol Network::Address::GetProtocol() const
{
    return m_protocol;
}

//------------------------------------------------------------------------------------------------

std::string const& Network::Address::GetUri() const
{
    return m_uri;
}

//------------------------------------------------------------------------------------------------

std::string_view const& Network::Address::GetScheme() const
{
    return m_scheme;
}

//------------------------------------------------------------------------------------------------

std::string_view const& Network::Address::GetAuthority() const
{
    return m_authority;
}

//------------------------------------------------------------------------------------------------

std::size_t Network::Address::GetSize() const
{
    return m_uri.size();
}

//------------------------------------------------------------------------------------------------

bool Network::Address::IsValid() const
{
    return !m_uri.empty();
}

//------------------------------------------------------------------------------------------------

bool Network::Address::CacheAddressPartitions()
{
    auto begin = m_uri.begin();
    auto end = m_uri.end();

    // The uri may not contain any whitespace characters. 
    if (m_uri.empty()) { return false; }
    if (bool const whitespace = std::any_of(begin, end, [] (auto c) { return std::isspace(c); });
        whitespace) { return false; }

    // Cache the scheme section of the uri. (e.g. <tcp>://127.0.0.1:1024)
    std::size_t schemeStart = m_uri.find(Network::SchemeSeperator);
    
    // If there is no scheme, append the prepend one to the 
    if (schemeStart == std::string::npos) {
        std::ostringstream oss;
        std::string_view scheme;
        switch (m_protocol) {
            case Protocol::TCP: { scheme = Network::TCP::Scheme; } break;
            case Protocol::LoRa: { scheme = Network::LoRa::Scheme; } break;
            default: assert(false); return false;
        }
        oss << scheme << Network::SchemeSeperator << m_uri;
        m_uri = oss.str();

        begin = m_uri.begin();
        end = m_uri.end();
        schemeStart = Network::TCP::Scheme.size();
    }

    // Cache the scheme parition of the uri. 
    m_scheme = { m_uri.begin(), m_uri.begin() + schemeStart };
    begin += schemeStart + Network::SchemeSeperator.size();

    std::size_t seperatorStart = m_uri.find_last_of(Network::ComponentSeperator);
    if (seperatorStart == std::string::npos) { return false; }
    seperatorStart -= schemeStart + Network::SchemeSeperator.size();

    // Cache the authority parition of the uri. (e.g. tcp://<127.0.0.1:1024>)
    m_authority = { begin, end };

    // Cache the primary address parition of the uri. (e.g. tcp://<127.0.0.1>:1024)
    m_primary = { begin, begin + seperatorStart };

    // Cache the secondary address parition of the uri. (e.g. tcp://127.0.0.1:<1024>)
    m_secondary = { begin + seperatorStart + 1, end };

    // Validate the uri based on the protocol type. 
    bool validated = false;
    switch (m_protocol) {
        case Protocol::TCP: {
            auto const result = Socket::ParseAddressType(*this);
            if (result != Socket::Type::Invalid) { validated = true; }
        } break;
        default: return false;
    }

    return validated;
}

//------------------------------------------------------------------------------------------------

void Network::Address::Reset()
{
    // Clear the partition views to avoid dangling pointers. 
    m_scheme = {};
    m_authority = {};
    m_primary = {};
    m_secondary = {};
    m_uri.clear();
    m_bootstrapable = false;
}

//------------------------------------------------------------------------------------------------
// } Network::Address
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Network::BindingAddress {
//------------------------------------------------------------------------------------------------

Network::BindingAddress::BindingAddress()
    : Address()
    , m_interface()
{
}

//------------------------------------------------------------------------------------------------

Network::BindingAddress::BindingAddress(
    Protocol protocol, std::string_view uri, std::string_view interface)
    : Address(protocol, local::BuildBindingUri(protocol, uri, interface), false)
    , m_interface(interface)
{
}

//------------------------------------------------------------------------------------------------

bool Network::BindingAddress::operator==(BindingAddress const& other) const
{
    return (m_interface == other.m_interface) && Address::operator==(other);
}

//------------------------------------------------------------------------------------------------

bool Network::BindingAddress::operator!=(BindingAddress const& other) const
{
    return !operator==(other);
}

//------------------------------------------------------------------------------------------------

std::string const& Network::BindingAddress::GetInterface() const
{
    return m_interface;
}

//------------------------------------------------------------------------------------------------
// } Network::BindingAddress 
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Network::RemoteAddress {
//------------------------------------------------------------------------------------------------

Network::RemoteAddress::RemoteAddress()
    : Address()
{
}

//------------------------------------------------------------------------------------------------

Network::RemoteAddress::RemoteAddress(Protocol protocol, std::string_view uri, bool bootstrapable)
    : Address(protocol, uri, bootstrapable)
{
}

//------------------------------------------------------------------------------------------------

bool Network::RemoteAddress::operator==(RemoteAddress const& other) const
{
    return Address::operator==(other);
}

//------------------------------------------------------------------------------------------------

bool Network::RemoteAddress::operator!=(RemoteAddress const& other) const
{
    return !operator==(other);
}

//------------------------------------------------------------------------------------------------

bool Network::RemoteAddress::IsBootstrapable() const
{
    return m_bootstrapable && IsValid();
}

//------------------------------------------------------------------------------------------------
// Network::RemoteAddress {
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Network::SocketComponents {
//------------------------------------------------------------------------------------------------

Network::Socket::Type Network::Socket::ParseAddressType(Address const& address)
{
    if (address.m_protocol != Protocol::TCP) { return Type::Invalid; }

    // General URI and Partition Size Checks
    {
        constexpr std::size_t MinimumLength = 9;
        constexpr std::size_t MaximumLength = 53;
        if (auto const size = address.m_uri.size(); size < MinimumLength || size > MaximumLength) {
            return Type::Invalid;
        }
        if (address.m_primary.empty() || address.m_secondary.empty()) { return Type::Invalid; }
    }

    // Port Range Checks (Acceptable: 1 - 65535);
    {
        auto const& partition = address.m_secondary;
        if (partition[0] == '-') { return Type::Invalid; }
        try {
            constexpr std::uint16_t MinimumValue = 1;
            constexpr std::uint16_t MaximumValue = std::numeric_limits<std::uint16_t>::max();
            auto const port = boost::lexical_cast<std::uint16_t>(partition);
            if (port < MinimumValue || port > MaximumValue) { return Type::Invalid; }
        } catch (...) { return Type::Invalid; }
    }

    // IP Address Checks. IPv6 Addresses must be wrapped in brakets (i.e. [..]). This is done 
    // to explicitly distinguish them. 
    {
        auto const& partition = address.m_primary;

        // Unwrap the IPv6 address to pass to the address parser. 
        std::string check;
        if (partition[0] == '[' && partition[partition.size() - 1] == ']') {
            check = { partition.data() + 1, partition.size() - 2 };
        } else {
            check = { partition.data(), partition.size() };
        }

        boost::system::error_code error;
        auto const ip = boost::asio::ip::address::from_string(check, error);
        if (error) { return Type::Invalid; }

        if (ip.is_v4()) { return Type::IPv4; }
        if (ip.is_v6() && partition[0] == '[') { return Type::IPv6; }
    }

    return Type::Invalid;
}

//------------------------------------------------------------------------------------------------

Network::Socket::Components Network::Socket::GetAddressComponents(Address const& address)
{
    assert(address.GetProtocol() == Protocol::TCP);
    return { address.m_primary, address.m_secondary }; 
}

//------------------------------------------------------------------------------------------------

Network::Socket::Components::Components(
    std::string_view const& primary, std::string_view const& secondary)
    : ip(primary)
    , port(secondary)
{
}

//------------------------------------------------------------------------------------------------

std::string_view const& Network::Socket::Components::GetIPAddress() const
{
    return ip;
}

//------------------------------------------------------------------------------------------------

std::string_view const& Network::Socket::Components::GetPort() const
{
    return port;
}

//------------------------------------------------------------------------------------------------

std::uint16_t Network::Socket::Components::GetPortNumber() const
{
    try {
        return boost::lexical_cast<std::uint16_t>(port);
    } catch (...) {
        return 0;
    }
}

//------------------------------------------------------------------------------------------------
// } Network::SocketComponents
//------------------------------------------------------------------------------------------------

std::string local::BuildBindingUri(
    Network::Protocol protocol, std::string_view uri, std::string_view interface)
{
    if (protocol == Network::Protocol::Invalid || uri.empty() || interface.empty()) { return {}; }

    std::ostringstream binding;
    
    // Attempt to find the scheme partition of the provided uri. If it cannot be found add
    // the appriopiate scheme depending upon the protocol type. Otherwise, copy the scheme 
    // into the generated binding. 
    std::size_t const scheme = uri.find(Network::SchemeSeperator);
    if (scheme == std::string::npos) {
        switch (protocol) {
            case Network::Protocol::TCP: binding << Network::TCP::Scheme; break;
            case Network::Protocol::LoRa: binding << Network::LoRa::Scheme; break;
            default: assert(false); break; // What is this?
        }
        binding << Network::SchemeSeperator;
    } else {
        std::copy(
            uri.begin(), uri.begin() + scheme + Network::SchemeSeperator.size(),
            std::ostream_iterator<char>(binding));
    }

    // Attempt to find the primary and secondary component seperator. If it is not found, 
    // a binding uri cannot be generated. 
    std::size_t const seperator = uri.find_last_of(Network::ComponentSeperator);
    if (seperator == std::string::npos) { return {}; }

    // Determine if the provided uri contains a wildcard. If it does the automatically determine
    // the primary address for the interface. Otherwise copy the primary address into the
    // generated buffer. 
    if (std::size_t const wildcard = uri.find(Network::Wildcard); wildcard != std::string::npos) {
        switch (protocol) {
            case Network::Protocol::TCP: binding << local::GetInterfaceAddress(interface); break;
            case Network::Protocol::LoRa: binding << ""; break;
            default: assert(false); break; // What is this?
        }
    } else {
        std::size_t const start = (scheme != std::string::npos) ?
            scheme + Network::SchemeSeperator.size() : 0;
        std::copy(
            uri.begin() + start, uri.begin() + seperator, std::ostream_iterator<char>(binding));
    }

    // Copy the remaining uri content into the generated buffer. 
    std::copy(uri.begin() + seperator, uri.end(), std::ostream_iterator<char>(binding));

    return binding.str();
}

//------------------------------------------------------------------------------------------------

std::string local::GetInterfaceAddress(std::string_view interface)
{
    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;
    void* tmpAddrPtr = nullptr;

    getifaddrs(&ifAddrStruct);

    std::string address;
    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) { continue; }

        if (ifa->ifa_addr->sa_family == AF_INET) {
            tmpAddrPtr = &(reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr)->sin_addr);
            char addressBuffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            if (std::string(ifa->ifa_name).find(interface) == 0) {
                address = std::string(addressBuffer);
                break;
            }
        }
    }

    if (ifAddrStruct != nullptr) { freeifaddrs(ifAddrStruct); }

    return address;
}

//------------------------------------------------------------------------------------------------
