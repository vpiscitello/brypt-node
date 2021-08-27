//----------------------------------------------------------------------------------------------------------------------
// File: Address.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "Address.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "LoRa/EndpointDefinitions.hpp"
#include "TCP/EndpointDefinitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <boost/lexical_cast.hpp>
#include <boost/asio/ip/address.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::string BuildBindingUri(
    Network::Protocol protocol, std::string_view uri, std::string_view interface);
std::string GetInterfaceAddress(std::string_view interface);

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Network::Address {
//----------------------------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------------------------

Network::Address& Network::Address::operator=(Address const& other)
{
    m_protocol = other.m_protocol;
    m_uri = other.m_uri;
    m_bootstrapable = other.m_bootstrapable;
    if (!CacheAddressPartitions()) { Reset(); }
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------------------------

Network::Address& Network::Address::operator=(Address&& other)
{
    m_protocol = std::move(other.m_protocol);
    m_uri = std::move(other.m_uri);
    m_scheme = std::move(other.m_scheme);
    m_authority = std::move(other.m_authority);
    m_primary = std::move(other.m_primary);
    m_secondary = std::move(other.m_secondary);
    m_bootstrapable = std::move(other.m_bootstrapable);
    other.Reset();
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Address::Address(Address&& other)
    : m_protocol(std::move(other.m_protocol))
    , m_uri(std::move(other.m_uri))
    , m_scheme(std::move(other.m_scheme))
    , m_authority(std::move(other.m_authority))
    , m_primary(std::move(other.m_primary))
    , m_secondary(std::move(other.m_secondary))
    , m_bootstrapable(std::move(other.m_bootstrapable))
{
    other.Reset();
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Network::Address::operator<=>(Address const& other) const
{
    // We can skip a string compare if the protocols don't match
    auto result = m_protocol <=> other.m_protocol;
    if (result == std::strong_ordering::equal) { return m_uri <=> other.m_uri; }
    return result;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Address::operator==(Address const& other) const
{
    // We can skip a string compare if the protocols don't match
    if (m_protocol == other.m_protocol) { return m_uri == other.m_uri; }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Address::operator!=(Address const& other) const
{
    return !operator==(other);
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Network::Address::GetProtocol() const
{
    return m_protocol;
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Network::Address::GetUri() const
{
    return m_uri;
}

//----------------------------------------------------------------------------------------------------------------------

std::string_view const& Network::Address::GetScheme() const
{
    return m_scheme;
}

//----------------------------------------------------------------------------------------------------------------------

std::string_view const& Network::Address::GetAuthority() const
{
    return m_authority;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Address::GetSize() const
{
    return m_uri.size();
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Address::IsValid() const
{
    return !m_uri.empty();
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Address::CacheAddressPartitions()
{
    if (m_uri.empty()) { return false; }

    // The uri may not contain any whitespace characters. 
    if (bool const spaces = std::any_of(
        m_uri.begin(), m_uri.end(), [] (auto c) { return std::isspace(c); }); spaces) { return false; }

    auto const schemeBoundary = GetSchemeBoundary();
    m_scheme = { m_uri.begin(), m_uri.begin() + schemeBoundary }; // Cache representation: <tcp>://127.0.0.1:1024.

    // Check to ensure the provided uri has a component seperator that we can parition. 
    std::size_t componentBoundary = m_uri.find_last_of(Network::ComponentSeperator);
    // Ensure the component boundary can be used to parition the string. 
    if (componentBoundary == std::string::npos || componentBoundary == m_uri.size()) { return false; }

    std::string::const_iterator const primary = m_uri.begin() + (m_scheme.size() + Network::SchemeSeperator.size());
    std::string::const_iterator const secondary = m_uri.begin() + componentBoundary + 1;
    m_authority = { primary, m_uri.end() }; // Cache representation: e.g. tcp://<127.0.0.1:1024>.
    m_primary = { primary, secondary - 1 }; // Cache representation: e.g. tcp://<127.0.0.1>:1024.
    m_secondary = { secondary, m_uri.end() }; // Cache representation:e.g. tcp://127.0.0.1:<1024>.

    // Validate the uri based on the protocol type. 
    switch (m_protocol) {
        case Protocol::TCP: return (Socket::ParseAddressType(*this) != Socket::Type::Invalid);
        case Protocol::LoRa: return true;
        default: return false;
    }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Address::GetSchemeBoundary()
{
    std::size_t end = m_uri.find(Network::SchemeSeperator);
    return (end != std::string::npos) ? end : PrependScheme(); // If there is no scheme, append the prepend one.
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Address::PrependScheme()
{
    std::ostringstream oss;
    std::string_view scheme;
    switch (m_protocol) {
        case Protocol::TCP: { scheme = Network::TCP::Scheme; } break;
        case Protocol::LoRa: { scheme = Network::LoRa::Scheme; } break;
        default: assert(false); return 0;
    }
    oss << scheme << Network::SchemeSeperator << m_uri; // <scheme>://<uri>
    m_uri = oss.str(); // Replace the uri with the scheme prepended. 
    return scheme.size(); // Notify the caller of the number of characters appended. 
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Address::Reset()
{
    // Clear the partition views to avoid dangling pointers. 
    m_protocol = Protocol::Invalid;
    m_scheme = {};
    m_authority = {};
    m_primary = {};
    m_secondary = {};
    m_uri.clear();
    m_bootstrapable = false;
}

//----------------------------------------------------------------------------------------------------------------------
// } Network::Address
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Network::BindingAddress {
//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress::BindingAddress()
    : Address()
    , m_interface()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress::BindingAddress(
    Protocol protocol, std::string_view uri, std::string_view interface)
    : Address(protocol, local::BuildBindingUri(protocol, uri, interface), false)
    , m_interface(interface)
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Network::BindingAddress::operator<=>(BindingAddress const& other) const
{
    // The base address comparator takes precedences over the interface result. 
    auto result = Address::operator<=>(other);
    if (result == std::strong_ordering::equal) { return m_interface <=> other.m_interface; }
    return result;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::BindingAddress::operator==(BindingAddress const& other) const
{
    if (Address::operator==(other)) { return m_interface == other.m_interface; }
    return false;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::BindingAddress::operator!=(BindingAddress const& other) const
{
    return !operator==(other);
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Network::BindingAddress::GetInterface() const
{
    return m_interface;
}

//----------------------------------------------------------------------------------------------------------------------
// } Network::BindingAddress 
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Network::RemoteAddress {
//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress::RemoteAddress()
    : Address()
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress::RemoteAddress(Protocol protocol, std::string_view uri, bool bootstrapable, Origin origin)
    : Address(protocol, uri, bootstrapable)
    , m_origin(origin)
{
}

//----------------------------------------------------------------------------------------------------------------------

std::strong_ordering Network::RemoteAddress::operator<=>(RemoteAddress const& other) const
{
    return Address::operator<=>(other);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::RemoteAddress::operator==(RemoteAddress const& other) const
{
    return Address::operator==(other);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::RemoteAddress::operator!=(RemoteAddress const& other) const
{
    return !operator==(other);
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::RemoteAddress::IsBootstrapable() const
{
    return m_bootstrapable && IsValid();
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress::Origin Network::RemoteAddress::GetOrigin() const
{
    return m_origin;
}

//----------------------------------------------------------------------------------------------------------------------
// } Network::RemoteAddress
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Network::SocketComponents {
//----------------------------------------------------------------------------------------------------------------------

Network::Socket::Type Network::Socket::ParseAddressType(Address const& address)
{
    if (address.m_protocol != Protocol::TCP) { return Type::Invalid; }
    if (!IsValidAddressSize(address)) { return Type::Invalid; }
    if (!IsValidPortNumber(address.m_secondary)) { return Type::Invalid; }
    return ParseAddressType(address.m_primary);
}

//----------------------------------------------------------------------------------------------------------------------

Network::Socket::Type Network::Socket::ParseAddressType(std::string_view const& partition)
{
    // IPv6 Addresses must be wrapped with [..]. This is done to explicitly distinguish the types. 
    // A copy of the partition must be made boost::asio only accepts null terminated strings. 
    auto const check = (partition[0] == '[' && partition[partition.size() - 1] == ']') ? 
        std::string { partition.data() + 1, partition.size() - 2 } :
        std::string { partition.data(), partition.size() };

    boost::system::error_code error;
    auto const ip = boost::asio::ip::address::from_string(check, error);
    if (error) { return Type::Invalid; }

    if (ip.is_v4()) { return Type::IPv4; }
    if (partition[0] == '[' && ip.is_v6()) { return Type::IPv6; }

    return Type::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Socket::IsValidAddressSize(Address const& address)
{
    constexpr std::size_t MinimumLength = 9;
    constexpr std::size_t MaximumLength = 53;
    if (address.m_primary.empty() || address.m_secondary.empty()) { return false; }
    bool const isOutOfRange = (address.m_uri.size() < MinimumLength || address.m_uri.size() > MaximumLength);
    return !isOutOfRange;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Socket::IsValidPortNumber(std::string_view const& partition)
{
    if (partition[0] == '-') { return false; } // Quickly check that the port is not a negative number. 

    // Port Range Checks (Acceptable: 1 - 65535);
    constexpr std::uint16_t MinimumValue = 1;
    constexpr std::uint16_t MaximumValue = std::numeric_limits<std::uint16_t>::max();
    try {
        auto const port = boost::lexical_cast<std::uint16_t>(partition);
        if (port < MinimumValue || port > MaximumValue) { return false; }
    } catch(boost::bad_lexical_cast& exception) {
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Socket::Components Network::Socket::GetAddressComponents(Address const& address)
{
    assert(address.GetProtocol() == Protocol::TCP);
    return { address.m_primary, address.m_secondary }; 
}

//----------------------------------------------------------------------------------------------------------------------

Network::Socket::Components::Components(std::string_view const& primary, std::string_view const& secondary)
    : ip(primary)
    , port(secondary)
{
}

//----------------------------------------------------------------------------------------------------------------------

std::string_view const& Network::Socket::Components::GetIPAddress() const
{
    return ip;
}

//----------------------------------------------------------------------------------------------------------------------

std::string_view const& Network::Socket::Components::GetPort() const
{
    return port;
}

//----------------------------------------------------------------------------------------------------------------------

std::uint16_t Network::Socket::Components::GetPortNumber() const
{
    try { return boost::lexical_cast<std::uint16_t>(port); }
    catch (...) { return 0; }
}

//----------------------------------------------------------------------------------------------------------------------
// } Network::SocketComponents
//----------------------------------------------------------------------------------------------------------------------

std::string local::BuildBindingUri(Network::Protocol protocol, std::string_view uri, std::string_view interface)
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
        std::size_t const start = (scheme != std::string::npos) ? scheme + Network::SchemeSeperator.size() : 0;
        std::copy(uri.begin() + start, uri.begin() + seperator, std::ostream_iterator<char>(binding));
    }

    // Copy the remaining uri content into the generated buffer. 
    std::copy(uri.begin() + seperator, uri.end(), std::ostream_iterator<char>(binding));

    return binding.str();
}

//----------------------------------------------------------------------------------------------------------------------

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

//----------------------------------------------------------------------------------------------------------------------
