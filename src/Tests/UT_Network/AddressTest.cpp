//----------------------------------------------------------------------------------------------------------------------
#include "Components/Network/Address.hpp"
#include "Components/Network/Protocol.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <gtest/gtest.h>
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------


//----------------------------------------------------------------------------------------------------------------------
} // local namespace
//----------------------------------------------------------------------------------------------------------------------
namespace test {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view TcpInterface = "lo";
using TcpExpectations = std::vector<std::tuple<std::string, std::string, Network::Socket::Type, bool>>;

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkAddressSuite, TcpBindingAddressValidationTest)
{
    test::TcpExpectations const expectations = {
        { "tcp://127.0.0.1:35216", "tcp://127.0.0.1:35216", Network::Socket::Type::IPv4, true },
        { "127.0.0.1:35216", "tcp://127.0.0.1:35216", Network::Socket::Type::IPv4, true },
        { "0.0.0.0:35216", "tcp://0.0.0.0:35216", Network::Socket::Type::IPv4, true },
        { "1.1.1.0:35216", "tcp://1.1.1.0:35216", Network::Socket::Type::IPv4, true },
        { "1.1.1.1:35216", "tcp://1.1.1.1:35216", Network::Socket::Type::IPv4, true },
        { "1.160.10.240:35216", "tcp://1.160.10.240:35216", Network::Socket::Type::IPv4, true },
        { "192.168.1.1:35216", "tcp://192.168.1.1:35216", Network::Socket::Type::IPv4, true },
        { "255.160.0.34:35216", "tcp://255.160.0.34:35216", Network::Socket::Type::IPv4, true },
        { "255.1.255.1:35216", "tcp://255.1.255.1:35216", Network::Socket::Type::IPv4, true },
        { "255.255.255.255:35216", "tcp://255.255.255.255:35216", Network::Socket::Type::IPv4, true },
        { "[::ffff:127.0.0.1]:35216", "tcp://[::ffff:127.0.0.1]:35216", Network::Socket::Type::IPv6, true },
        { "tcp://[::ffff:127.0.0.1]:35216", "tcp://[::ffff:127.0.0.1]:35216", Network::Socket::Type::IPv6, true },
        { "[::ffff:192.168.0.1]:35216", "tcp://[::ffff:192.168.0.1]:35216", Network::Socket::Type::IPv6, true },
        { "[0:0:0:0:0:0:0:0]:35216", "tcp://[0:0:0:0:0:0:0:0]:35216", Network::Socket::Type::IPv6, true },
        { "[0:0:0:0:1:0:0:0]:35216", "tcp://[0:0:0:0:1:0:0:0]:35216", Network::Socket::Type::IPv6, true },
        { "[::]:35216", "tcp://[::]:35216", Network::Socket::Type::IPv6, true },
        { "[0::]:35216", "tcp://[0::]:35216", Network::Socket::Type::IPv6, true },
        { "[ffff::]:35216", "tcp://[ffff::]:35216", Network::Socket::Type::IPv6, true },
        { "[::1]:35216", "tcp://[::1]:35216", Network::Socket::Type::IPv6, true },
        { "[1080::8:800:200c:417a]:35216", "tcp://[1080::8:800:200c:417a]:35216", Network::Socket::Type::IPv6, true },
        { "[2001:0db8::1428:57ab]:35216", "tcp://[2001:0db8::1428:57ab]:35216", Network::Socket::Type::IPv6, true },
        { "[0000:0000:0000:0000:0000:0000:0000:0000]:35216", "tcp://[0000:0000:0000:0000:0000:0000:0000:0000]:35216", Network::Socket::Type::IPv6, true },
        { "[fe80:0000:0000:0000:0204:61ff:fe9d:f156]:35216", "tcp://[fe80:0000:0000:0000:0204:61ff:fe9d:f156]:35216", Network::Socket::Type::IPv6, true },
        { "[2001:0000:4136:e378:8000:63bf:3fff:fdd2]:35216", "tcp://[2001:0000:4136:e378:8000:63bf:3fff:fdd2]:35216", Network::Socket::Type::IPv6, true },
        { "[2001:0db8:1234:ffff:ffff:ffff:ffff:ffff]:35216", "tcp://[2001:0db8:1234:ffff:ffff:ffff:ffff:ffff]:35216", Network::Socket::Type::IPv6, true },
        { "[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:35216", "tcp://[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:35216", Network::Socket::Type::IPv6, true },
        { "", "", Network::Socket::Type::Invalid, false },
        { "tcp://", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1", "", Network::Socket::Type::Invalid, false },
        { "abcd", "", Network::Socket::Type::Invalid, false },
        { "ipaddress", "", Network::Socket::Type::Invalid, false },
        { "-1", "", Network::Socket::Type::Invalid, false },
        { "          ", "", Network::Socket::Type::Invalid, false },
        { "          :", "", Network::Socket::Type::Invalid, false },
        { "          :35216", "", Network::Socket::Type::Invalid, false },
        { " . . . :35216", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1:    ", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1  :35216", "", Network::Socket::Type::Invalid, false },
        { "127...:35216", "", Network::Socket::Type::Invalid, false },
        { " 127.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1:35216:35216", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1 127.0.0.1", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1 35216", "", Network::Socket::Type::Invalid, false },
        { "127,0,0,1:35216", "", Network::Socket::Type::Invalid, false },
        { "127:0:0:1:35216", "", Network::Socket::Type::Invalid, false },
        { "-127.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "+127.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "127.-0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1:-35216", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1:352-16", "", Network::Socket::Type::Invalid, false },
        { "-127.0.0.1:-35216", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.256:35216", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1.:35216", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1.1:35216", "", Network::Socket::Type::Invalid, false },
        { "127.00.a.1:35216", "", Network::Socket::Type::Invalid, false },
        { "18446744073709551616", "", Network::Socket::Type::Invalid, false },
        { "1844674407370955161618446744073709551616", "", Network::Socket::Type::Invalid, false },
        { "18446744073709551616.18446744073709551616.18446744073709551616.18446744073709551616:18446744073709551616", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1:18446744073709551616", "", Network::Socket::Type::Invalid, false },
        { "...:", "", Network::Socket::Type::Invalid, false },
        { "1 1 1 1:35216", "", Network::Socket::Type::Invalid, false },
        { "1,1,1,1:35216", "", Network::Socket::Type::Invalid, false },
        { "1.1.1.1e-80:35216", "", Network::Socket::Type::Invalid, false },
        { "':10.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "[:::::::]:", "", Network::Socket::Type::Invalid, false },
        { "::ffff:127.0.0.1:35216", "", Network::Socket::Type::Invalid, false  },
        { "[::ffff:127.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "[::ffff:127.0.0.1:35216]", "", Network::Socket::Type::Invalid, false },
        { "[][::ffff:127.0.0.1]:35216", "", Network::Socket::Type::Invalid, false },
        { "[][::ffff:127.0.0.1:35216]", "", Network::Socket::Type::Invalid, false },
        { "[127:0:0:1]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[127:0:0:1]:35216", "", Network::Socket::Type::Invalid, false },
        { "[::1 ::1]:35216", "", Network::Socket::Type::Invalid, false },
        { "[:::1.2.3.4]:35216", "", Network::Socket::Type::Invalid, false },
        { "[a::g]:35216", "", Network::Socket::Type::Invalid, false },
        { "[1.2.3.4:1111:2222:3333:4444::5555]:35216", "", Network::Socket::Type::Invalid, false },
        { "[11111111:3333:4444:5555:6666:7777:8888]:35216", "", Network::Socket::Type::Invalid, false },
        { "ffgg:ffff:ffff:ffff:ffff:ffff:ffff:ffff", "", Network::Socket::Type::Invalid, false },
        { "[ffgg:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:35216", "", Network::Socket::Type::Invalid, false }
    };

    for (auto const& [input, expected, type, valid] : expectations) {
        Network::BindingAddress const address(Network::Protocol::TCP, input, test::TcpInterface);
        EXPECT_EQ(address.GetProtocol(), (valid) ? Network::Protocol::TCP : Network::Protocol::Invalid);
        EXPECT_EQ(address.GetUri(), expected);
        EXPECT_EQ(address.GetSize(), expected.size());
        EXPECT_EQ(address.IsValid(), valid);
        EXPECT_EQ(address.GetInterface(), test::TcpInterface);
        EXPECT_EQ(Network::Socket::ParseAddressType(address), type);

        if (!expected.empty()) {
            EXPECT_EQ(address.GetScheme(), expected.substr(0, 3));
            EXPECT_EQ(address.GetAuthority(), expected.substr(6));
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkAddressSuite, TcpBindingAddressComponentTest)
{
    // Interface Address
    {
        // The interface could be matched to an IPv4 or IPv6 address depending on the system running the test. 
        constexpr std::string_view ExpectedIPv4Address = "127.0.0.1";
        constexpr std::string_view ExpectedIPv6Address = "[::1%0]";

        Network::BindingAddress const address( Network::Protocol::TCP, "*:35216", test::TcpInterface);
        auto const components = Network::Socket::GetAddressComponents(address);
        auto const& [ip, port] = components;
        
        EXPECT_TRUE((ip == ExpectedIPv4Address) || (ip == ExpectedIPv6Address));
        EXPECT_EQ(port, "35216");
        EXPECT_EQ(components.GetPortNumber(), 35216);
    }

    // IPv6 Address
    {
        Network::BindingAddress const address(Network::Protocol::TCP, "[::ffff:127.0.0.1]:35216", test::TcpInterface);
        auto const components = Network::Socket::GetAddressComponents(address);
        auto const& [ip, port] = components;

        EXPECT_EQ(ip, "[::ffff:127.0.0.1]");
        EXPECT_EQ(port, "35216");
        EXPECT_EQ(components.GetPortNumber(), 35216);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkAddressSuite, TcpBindingAddressMoveTest)
{
    // The interface could be matched to an IPv4 or IPv6 address depending on the system running the test. 
    constexpr std::string_view ExpectedIPv4Uri = "tcp://127.0.0.1:35216";
    constexpr std::string_view ExpectedIPv4Authority = "127.0.0.1:35216";
    constexpr std::string_view ExpectedIPv6Uri = "tcp://[::1%0]:35216";
    constexpr std::string_view ExpectedIPv6Authority = "[::1%0]:35216";

    Network::BindingAddress initial(Network::Protocol::TCP, "*:35216", test::TcpInterface);
    Network::BindingAddress address(std::move(initial));

    EXPECT_EQ(address.GetProtocol(), Network::Protocol::TCP);
    EXPECT_TRUE((address.GetUri() == ExpectedIPv4Uri) || (address.GetUri() == ExpectedIPv6Uri));
    EXPECT_EQ(address.GetScheme(), "tcp");
    EXPECT_TRUE((address.GetAuthority() == ExpectedIPv4Authority) || (address.GetAuthority() == ExpectedIPv6Authority));
    EXPECT_TRUE((address.GetSize() == ExpectedIPv4Uri.size()) || (address.GetSize() == ExpectedIPv6Uri.size()));
    EXPECT_EQ(address.IsValid(), true);
    EXPECT_EQ(address.GetInterface(), test::TcpInterface);
    EXPECT_NE(Network::Socket::ParseAddressType(address), Network::Socket::Type::Invalid);
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkAddressSuite, TcpRemoteAddressValidationTest)
{
    test::TcpExpectations const expectations = {
        { "tcp://127.0.0.1:35216", "tcp://127.0.0.1:35216", Network::Socket::Type::IPv4, true },
        { "127.0.0.1:35216", "tcp://127.0.0.1:35216", Network::Socket::Type::IPv4, true },
        { "0.0.0.0:35216", "tcp://0.0.0.0:35216", Network::Socket::Type::IPv4, true },
        { "1.1.1.0:35216", "tcp://1.1.1.0:35216", Network::Socket::Type::IPv4, true },
        { "1.1.1.1:35216", "tcp://1.1.1.1:35216", Network::Socket::Type::IPv4, true },
        { "1.160.10.240:35216", "tcp://1.160.10.240:35216", Network::Socket::Type::IPv4, true },
        { "192.168.1.1:35216", "tcp://192.168.1.1:35216", Network::Socket::Type::IPv4, true },
        { "255.160.0.34:35216", "tcp://255.160.0.34:35216", Network::Socket::Type::IPv4, true },
        { "255.1.255.1:35216", "tcp://255.1.255.1:35216", Network::Socket::Type::IPv4, true },
        { "255.255.255.255:35216", "tcp://255.255.255.255:35216", Network::Socket::Type::IPv4, true },
        { "[::ffff:127.0.0.1]:35216", "tcp://[::ffff:127.0.0.1]:35216", Network::Socket::Type::IPv6, true },
        { "tcp://[::ffff:127.0.0.1]:35216", "tcp://[::ffff:127.0.0.1]:35216", Network::Socket::Type::IPv6, true },
        { "[::ffff:192.168.0.1]:35216", "tcp://[::ffff:192.168.0.1]:35216", Network::Socket::Type::IPv6, true },
        { "[0:0:0:0:0:0:0:0]:35216", "tcp://[0:0:0:0:0:0:0:0]:35216", Network::Socket::Type::IPv6, true },
        { "[0:0:0:0:1:0:0:0]:35216", "tcp://[0:0:0:0:1:0:0:0]:35216", Network::Socket::Type::IPv6, true },
        { "[::]:35216", "tcp://[::]:35216", Network::Socket::Type::IPv6, true },
        { "[0::]:35216", "tcp://[0::]:35216", Network::Socket::Type::IPv6, true },
        { "[ffff::]:35216", "tcp://[ffff::]:35216", Network::Socket::Type::IPv6, true },
        { "[::1]:35216", "tcp://[::1]:35216", Network::Socket::Type::IPv6, true },
        { "[1080::8:800:200c:417a]:35216", "tcp://[1080::8:800:200c:417a]:35216", Network::Socket::Type::IPv6, true },
        { "[2001:0db8::1428:57ab]:35216", "tcp://[2001:0db8::1428:57ab]:35216", Network::Socket::Type::IPv6, true },
        { "[0000:0000:0000:0000:0000:0000:0000:0000]:35216", "tcp://[0000:0000:0000:0000:0000:0000:0000:0000]:35216", Network::Socket::Type::IPv6, true },
        { "[fe80:0000:0000:0000:0204:61ff:fe9d:f156]:35216", "tcp://[fe80:0000:0000:0000:0204:61ff:fe9d:f156]:35216", Network::Socket::Type::IPv6, true },
        { "[2001:0000:4136:e378:8000:63bf:3fff:fdd2]:35216", "tcp://[2001:0000:4136:e378:8000:63bf:3fff:fdd2]:35216", Network::Socket::Type::IPv6, true },
        { "[2001:0db8:1234:ffff:ffff:ffff:ffff:ffff]:35216", "tcp://[2001:0db8:1234:ffff:ffff:ffff:ffff:ffff]:35216", Network::Socket::Type::IPv6, true },
        { "[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:35216", "tcp://[ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:35216", Network::Socket::Type::IPv6, true },
        { "*:35216", "", Network::Socket::Type::Invalid, false  },
        { "tcp://*:35216", "", Network::Socket::Type::Invalid, false  },
        { "", "", Network::Socket::Type::Invalid, false },
        { "tcp://", "", Network::Socket::Type::Invalid, false },
        { "127.0.0.1", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1", "", Network::Socket::Type::Invalid, false },
        { "abcd", "", Network::Socket::Type::Invalid, false },
        { "ipaddress", "", Network::Socket::Type::Invalid, false },
        { "-1", "", Network::Socket::Type::Invalid, false },
        { "          ", "", Network::Socket::Type::Invalid, false },
        { "tcp://          :", "", Network::Socket::Type::Invalid, false },
        { "tcp://          :35216", "", Network::Socket::Type::Invalid, false },
        { "tcp:// . . . :35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1:    ", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1  :35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127...:35216", "", Network::Socket::Type::Invalid, false },
        { " tcp://127.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1:35216:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1 127.0.0.1", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1 35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127,0,0,1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127:0:0:1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://-127.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://+127.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.-0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1:-35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1:352-16", "", Network::Socket::Type::Invalid, false },
        { "tcp://-127.0.0.1:-35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.256:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1.:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1.1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.00.a.1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://18446744073709551616", "", Network::Socket::Type::Invalid, false },
        { "tcp://1844674407370955161618446744073709551616", "", Network::Socket::Type::Invalid, false },
        { "tcp://18446744073709551616.18446744073709551616.18446744073709551616.18446744073709551616:18446744073709551616", "", Network::Socket::Type::Invalid, false },
        { "tcp://127.0.0.1:18446744073709551616", "", Network::Socket::Type::Invalid, false },
        { "tcp://...:", "", Network::Socket::Type::Invalid, false },
        { "tcp://1 1 1 1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://1,1,1,1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://1.1.1.1e-80:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://':10.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[:::::::]:", "", Network::Socket::Type::Invalid, false },
        { "::ffff:127.0.0.1:35216", "", Network::Socket::Type::Invalid, false  },
        { "tcp://[::ffff:127.0.0.1:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[::ffff:127.0.0.1:35216]", "", Network::Socket::Type::Invalid, false },
        { "tcp://[][::ffff:127.0.0.1]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[][::ffff:127.0.0.1:35216]", "", Network::Socket::Type::Invalid, false },
        { "tcp://[127:0:0:1]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://tcp://[127:0:0:1]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[::1 ::1]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[:::1.2.3.4]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[a::g]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[1.2.3.4:1111:2222:3333:4444::5555]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://[11112222:3333:4444:5555:6666:7777:8888]:35216", "", Network::Socket::Type::Invalid, false },
        { "tcp://ffgg:ffff:ffff:ffff:ffff:ffff:ffff:ffff", "", Network::Socket::Type::Invalid, false },
        { "tcp://[ffgg:ffff:ffff:ffff:ffff:ffff:ffff:ffff]:35216", "", Network::Socket::Type::Invalid, false }
    };

    for (auto const& [input, expected, type, valid] : expectations) {
        Network::RemoteAddress const address(Network::Protocol::TCP, input, true);
        EXPECT_EQ(address.GetProtocol(), (valid) ? Network::Protocol::TCP : Network::Protocol::Invalid);
        EXPECT_EQ(address.GetUri(), expected);
        EXPECT_EQ(address.GetSize(), expected.size());
        EXPECT_EQ(address.IsValid(), valid);
        EXPECT_EQ(Network::Socket::ParseAddressType(address), type);

        if (!expected.empty()) {
            EXPECT_EQ(address.GetScheme(), expected.substr(0, 3));
            EXPECT_EQ(address.GetAuthority(), expected.substr(6));
        }

        EXPECT_EQ(address.IsBootstrapable(), (type != Network::Socket::Type::Invalid));
        EXPECT_EQ(address.GetOrigin(), Network::RemoteAddress::Origin::Network);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkAddressSuite, TcpRemoteAddressComponentTest)
{
    // IPv4 Address
    {
        Network::RemoteAddress const address(Network::Protocol::TCP, "127.0.0.1:35216", true);
        auto const components = Network::Socket::GetAddressComponents(address);
        auto const& [ip, port] = components;

        EXPECT_EQ(ip, "127.0.0.1");
        EXPECT_EQ(port, "35216");
        EXPECT_EQ(components.GetPortNumber(), 35216);
    }

    // IPv6 Address
    {
        Network::RemoteAddress const address(Network::Protocol::TCP, "[::ffff:127.0.0.1]:35216", true);
        auto const components = Network::Socket::GetAddressComponents(address);
        auto const& [ip, port] = components;

        EXPECT_EQ(ip, "[::ffff:127.0.0.1]");
        EXPECT_EQ(port, "35216");
        EXPECT_EQ(components.GetPortNumber(), 35216);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkAddressSuite, TcpRemoteAddressBootstrapableTest)
{
    using BootstrapExpectations = std::vector<std::tuple<std::string, bool, bool>>;
    BootstrapExpectations const expectations = {
        { "127.0.0.1:35216", true, true },
        { "[::ffff:127.0.0.1]:35216", true, true },
        { "127.0.0.1:35216", false, false },
        { "[::ffff:127.0.0.1]:35216", false, false }
    };

    for (auto const& [input, bootstrapable, expected] : expectations) {
        Network::RemoteAddress const address(Network::Protocol::TCP, input, bootstrapable);
        EXPECT_EQ(address.IsBootstrapable(), expected);
    }
}

//----------------------------------------------------------------------------------------------------------------------

TEST(NetworkAddressSuite, TcpRemoteAddressMoveTest)
{
    using Origin = Network::RemoteAddress::Origin;
    constexpr std::string_view Expected = "tcp://127.0.0.1:35216";
    Network::RemoteAddress initial(Network::Protocol::TCP, "127.0.0.1:35216", true, Origin::User);
    Network::RemoteAddress address(std::move(initial));

    EXPECT_EQ(address.GetProtocol(), Network::Protocol::TCP);
    EXPECT_EQ(address.GetUri(), Expected);
    EXPECT_EQ(address.GetScheme(), "tcp");
    EXPECT_EQ(address.GetAuthority(), "127.0.0.1:35216");
    EXPECT_EQ(address.GetSize(), Expected.size());
    EXPECT_EQ(address.IsValid(), true);
    EXPECT_TRUE(address.IsBootstrapable());
    EXPECT_EQ(initial.GetOrigin(), Origin::User);
    EXPECT_EQ(Network::Socket::ParseAddressType(address), Network::Socket::Type::IPv4);
}

//----------------------------------------------------------------------------------------------------------------------
