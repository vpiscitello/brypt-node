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
#include <boost/asio/ip/tcp.hpp>
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#if defined(WIN32)
#include <windows.h>
#include <iphlpapi.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::string BuildBindingUri(
    Network::Protocol protocol, std::string_view uri, std::string_view interface);
std::string GetInterfaceAddress(std::string_view interface);
std::string BuildRemoteAddress(boost::asio::ip::tcp::endpoint const& endpoint);

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

Network::Address::Address(Protocol protocol, std::string_view uri, bool bootstrapable, bool skipAddressValidation)
    : m_protocol(protocol)
    , m_uri(uri)
    , m_scheme()
    , m_authority()
    , m_primary()
    , m_secondary()
    , m_bootstrapable(bootstrapable)
{
    if (!CacheAddressPartitions(skipAddressValidation)) { Reset(); }
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
    CopyAddressPartitions(other);
}

//----------------------------------------------------------------------------------------------------------------------

Network::Address& Network::Address::operator=(Address const& other)
{
    m_protocol = other.m_protocol;
    m_uri = other.m_uri;
    m_bootstrapable = other.m_bootstrapable;
    CopyAddressPartitions(other);
    return *this;
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

bool Network::Address::CacheAddressPartitions(bool skipAddressValidation)
{
    if (m_uri.empty()) { return false; }

    // The uri may not contain any whitespace characters. 
    if (bool const spaces = std::any_of(
        m_uri.begin(), m_uri.end(), [] (auto c) { return std::isspace(c); }); spaces) { return false; }

    auto const schemeBoundary = GetSchemeBoundary();
    m_scheme = { m_uri.begin(), m_uri.begin() + schemeBoundary }; // Cache representation: <tcp>://127.0.0.1:1024.

    // Check to ensure the provided uri has a component separator that we can partition. 
    std::size_t componentBoundary = m_uri.find_last_of(Network::ComponentSeparator);
    // Ensure the component boundary can be used to partition the string. 
    if (componentBoundary == std::string::npos || componentBoundary == m_uri.size()) { return false; }

    std::string::const_iterator const primary = m_uri.begin() + (m_scheme.size() + Network::SchemeSeparator.size());
    std::string::const_iterator const secondary = m_uri.begin() + componentBoundary + 1;
    m_authority = { primary, m_uri.end() }; // Cache representation: e.g. tcp://<127.0.0.1:1024>.
    m_primary = { primary, secondary - 1 }; // Cache representation: e.g. tcp://<127.0.0.1>:1024.
    m_secondary = { secondary, m_uri.end() }; // Cache representation:e.g. tcp://127.0.0.1:<1024>.

    // If we need to validate the address, do so. We should only be skipping address validation if the address
    // was constructed from a trusted third-party resources (i.e. an actual socket already in use). 
    if (!skipAddressValidation) {
        switch (m_protocol) {
            case Protocol::TCP: return ( Socket::ParseAddressType(*this) != Socket::Type::Invalid );
            case Protocol::LoRa: return true;
            case Protocol::Test: return true;
            default: return false;
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------

void Network::Address::CopyAddressPartitions(Address const& other)
{
    if (m_uri.empty()) { return; }
    m_scheme = { m_uri.begin(), m_uri.begin() + other.m_scheme.size() };

    std::string::const_iterator const primary = m_uri.begin() + (m_scheme.size() + Network::SchemeSeparator.size());
    std::string::const_iterator const secondary = primary + other.m_primary.size() + 1;

    m_authority = { primary, m_uri.end()};
    m_primary = { primary, secondary - 1 };
    m_secondary = { secondary, m_uri.end() };
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Network::Address::GetSchemeBoundary()
{
    std::size_t end = m_uri.find(Network::SchemeSeparator);
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
        case Protocol::Test: { scheme = Network::TestScheme; } break;
        default: assert(false); return 0;
    }
    oss << scheme << Network::SchemeSeparator << m_uri; // <scheme>://<uri>
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

template<>
Network::BindingAddress Network::BindingAddress::CreateTestAddress<InvokeContext::Test>(
    std::string_view uri, std::string_view interface)
{
    return BindingAddress{ InvokeContext::Test, uri, interface };
}

//----------------------------------------------------------------------------------------------------------------------

Network::BindingAddress::BindingAddress(InvokeContext, std::string_view uri, std::string_view interface)
    : Address(Protocol::Test, uri, false)
    , m_interface(interface)
{
}

//----------------------------------------------------------------------------------------------------------------------
// } Network::BindingAddress 
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Network::RemoteAddress {
//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress::RemoteAddress()
    : Address()
    , m_origin(Origin::Invalid)
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress::RemoteAddress(Protocol protocol, std::string_view uri, bool bootstrapable, Origin origin)
    : Address(protocol, uri, bootstrapable)
    , m_origin(origin)
{
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress::RemoteAddress(boost::asio::ip::tcp::endpoint const& endpoint, bool bootstrapable, Origin origin)
    : Address(Protocol::TCP, local::BuildRemoteAddress(endpoint), bootstrapable, true)
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

template<>
Network::RemoteAddress Network::RemoteAddress::CreateTestAddress<InvokeContext::Test>(
    std::string_view uri, bool bootstrapable, Origin origin)
{
    return RemoteAddress{ InvokeContext::Test, uri, bootstrapable, origin };
}

//----------------------------------------------------------------------------------------------------------------------

Network::RemoteAddress::RemoteAddress(InvokeContext, std::string_view uri, bool bootstrapable, Origin origin)
    : Address(Protocol::Test, uri, bootstrapable)
    , m_origin(origin)
{
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
    // A copy of the partition must be made inet_pton only accepts null terminated strings. 
    bool const isIPv6Assumed = ( partition[0] == '[' && partition[partition.size() - 1] == ']' );
    
    std::string check = (isIPv6Assumed) ?
        std::string{ partition.begin() + 1, partition.end() - 1 } : 
        std::string{ partition.begin(), partition.end() };

    auto const family = (isIPv6Assumed) ? AF_INET6 : AF_INET;

    switch (family) {
        case AF_INET: {
            sockaddr_in address;
            if (auto const result = ::inet_pton(AF_INET, check.data(), &address.sin_addr); result > 0) { 
                return Type::IPv4;
            }
        } [[fallthrough]];
        case AF_INET6: {
            std::size_t boundary = check.find_last_of(Network::ScopeSeparator);
            if (boundary != std::string::npos) {
                check.erase(check.begin() + boundary, check.end());
            }

            sockaddr_in6 address;
            if (auto const result = ::inet_pton(family, check.data(), &address.sin6_addr); result > 0) { 
                return Type::IPv6;
            }
        } break;
    }

    return Type::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

bool Network::Socket::IsValidAddressSize(Address const& address)
{
    constexpr std::size_t MinimumLength = 9;
    constexpr std::size_t MaximumLength = 256;
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
    } catch(boost::bad_lexical_cast&) {
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
    // the appropriate scheme depending upon the protocol type. Otherwise, copy the scheme 
    // into the generated binding. 
    std::size_t const scheme = uri.find(Network::SchemeSeparator);
    if (scheme == std::string::npos) {
        switch (protocol) {
            case Network::Protocol::TCP: binding << Network::TCP::Scheme; break;
            case Network::Protocol::LoRa: binding << Network::LoRa::Scheme; break;
            case Network::Protocol::Test: binding << Network::TestScheme; break;
            default: assert(false); break; // What is this?
        }
        binding << Network::SchemeSeparator;
    } else {
        std::copy(
            uri.begin(), uri.begin() + scheme + Network::SchemeSeparator.size(),
            std::ostream_iterator<char>(binding));
    }

    // Attempt to find the primary and secondary component separator. If it is not found, 
    // a binding uri cannot be generated. 
    std::size_t const separator = uri.find_last_of(Network::ComponentSeparator);
    if (separator == std::string::npos) { return {}; }

    // Determine if the provided uri contains a wildcard. If it does the automatically determine
    // the primary address for the interface. Otherwise copy the primary address into the
    // generated buffer. 
    if (std::size_t const wildcard = uri.find(Network::Wildcard); wildcard != std::string::npos) {
        switch (protocol) {
            case Network::Protocol::TCP: binding << local::GetInterfaceAddress(interface); break;
            case Network::Protocol::LoRa:
            case Network::Protocol::Test: binding << "*"; break;
            default: assert(false); break; // What is this?
        }
    } else {
        std::size_t const start = (scheme != std::string::npos) ? scheme + Network::SchemeSeparator.size() : 0;
        if (start >= separator) { return {}; }
        std::copy(uri.begin() + start, uri.begin() + separator, std::ostream_iterator<char>(binding));
    }

    // Copy the remaining uri content into the generated buffer. 
    std::copy(uri.begin() + separator, uri.end(), std::ostream_iterator<char>(binding));

    return binding.str();
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::GetInterfaceAddress(std::string_view interface)
{
    constexpr auto AddressToString = [] (sockaddr const& socket) -> std::string {
        std::string address;
        switch (socket.sa_family) {
            case AF_INET: {
                auto const& converted = reinterpret_cast<sockaddr_in const&>( socket );
                std::array<char, INET_ADDRSTRLEN> buffer;
                ::inet_ntop(converted.sin_family, &converted.sin_addr, buffer.data(), buffer.size());
                address = std::string{ buffer.data() };
            } break;
            case AF_INET6: {
                auto const& converted = reinterpret_cast<sockaddr_in6 const&>( socket );
                std::array<char, INET6_ADDRSTRLEN> buffer;
                ::inet_ntop(converted.sin6_family, &converted.sin6_addr, buffer.data(), buffer.size());
                
                std::ostringstream oss;
                oss << "[" << buffer.data() << "%" << converted.sin6_scope_id << "]";
                address = oss.str();
            } break;
            default: assert(false); break; 
        }
        return address;
    };


#if defined(WIN32)
    constexpr UINT CodePage = CP_ACP;
    constexpr DWORD MultiByteFlags = 0;
    
    std::wstring multiByteInterface;
    if (interface == Network::LoopbackInterface) {
        multiByteInterface = L"Loopback Pseudo-Interface";
    } else {
        std::int32_t const interfaceSize = static_cast<std::int32_t>( interface.size() );
        std::int32_t const multiByteSize = MultiByteToWideChar(
            CodePage, MultiByteFlags, interface.data(), interfaceSize, nullptr, 0);
        if (multiByteSize <= 0) { return {}; }

        multiByteInterface.resize(multiByteSize + 1);
        std::int32_t const conversionResult = MultiByteToWideChar(
            CodePage, MultiByteFlags, interface.data(), interfaceSize, multiByteInterface.data(), multiByteSize);
        if (conversionResult <= 0) { return {}; }
    }

    constexpr ULONG DefaultBufferSize = 15'000;
    constexpr ULONG Family = AF_UNSPEC;
    constexpr ULONG AdapterFlags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    
    std::vector<std::uint8_t> buffer(DefaultBufferSize, 0x00);
    PIP_ADAPTER_ADDRESSES pAdapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    
    ULONG result = 0;
    ULONG size = DefaultBufferSize;
    for (;;) {
        result = GetAdaptersAddresses(Family, AdapterFlags, nullptr, pAdapterAddresses, &size);
        if (result != ERROR_BUFFER_OVERFLOW) { break; }
        buffer.resize(size);
        pAdapterAddresses = reinterpret_cast<PIP_ADAPTER_ADDRESSES>( buffer.data() );
    }

    if (result != ERROR_SUCCESS) { return {}; }

    for (auto pAdapter = pAdapterAddresses; pAdapter; pAdapter = pAdapter = pAdapter->Next) {
        if (pAdapter->OperStatus != IfOperStatusUp) { continue; }
        switch (pAdapter->IfType) {
            case IF_TYPE_IEEE80211:
            case IF_TYPE_ETHERNET_CSMACD: 
            case IF_TYPE_SOFTWARE_LOOPBACK: {
                if (std::wstring{ pAdapter->FriendlyName }.find(multiByteInterface) == 0) {
                    for (auto pAddress = pAdapter->FirstUnicastAddress; pAddress; pAddress = pAddress->Next) {
                        SOCKET_ADDRESS const& socket = pAddress->Address;
                        LPSOCKADDR const pSocketAddress = socket.lpSockaddr;
                        if (pSocketAddress->sa_family == AF_INET || pSocketAddress->sa_family == AF_INET6) {
                            return AddressToString(*pSocketAddress);
                        }
                    }
                    
                    return {}; // Stop parsing the adapters after matching the interface. 
                }
            } break;
            default: break;
        }
    }

    return {};
#else
    ifaddrs* pNetworkAddresses = nullptr;
    getifaddrs(&pNetworkAddresses);

    std::string address;
    for (auto pAddress = pNetworkAddresses; pAddress; pAddress = pAddress->ifa_next) {
        if (pAddress->ifa_addr) {
            if (pAddress->ifa_addr->sa_family == AF_INET || pAddress->ifa_addr->sa_family == AF_INET6) {
                if (std::string{ pAddress->ifa_name }.find(interface) == 0) {
                    address = AddressToString(*pAddress->ifa_addr);
                    break;
                }
            }
        }
    }

    if (!pNetworkAddresses) { freeifaddrs(pNetworkAddresses); }

    return address;
#endif
}

//----------------------------------------------------------------------------------------------------------------------

std::string local::BuildRemoteAddress(boost::asio::ip::tcp::endpoint const& endpoint)
{
    std::ostringstream uri;
    uri << Network::TCP::Scheme << Network::SchemeSeparator;
    if (auto const& address = endpoint.address(); address.is_v6()) {
        auto const ipv6 = address.to_v6();
        ipv6.scope_id();
        uri << "[" << ipv6.to_string()  << "%" << ipv6.scope_id() << "]";
    } else {
        uri << address.to_string();
    }
    uri << Network::ComponentSeparator << endpoint.port();
    return uri.str();
}

//----------------------------------------------------------------------------------------------------------------------
