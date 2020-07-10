//------------------------------------------------------------------------------------------------
// File: NetworkUtils.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace NetworkUtils {
//------------------------------------------------------------------------------------------------

using NetworkAddress = std::string;
using PortNumber = std::uint16_t;

constexpr std::string_view Wildcard = "*"; 
constexpr std::string_view ComponentSeperator = ":";
constexpr std::string_view SchemeSeperator = "://";

using AddressComponentPair = std::pair<std::string, std::string>;

AddressComponentPair SplitAddressString(std::string_view str);
NetworkAddress GetInterfaceAddress(std::string_view interface);

//------------------------------------------------------------------------------------------------
} // NetworkUtils namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
inline NetworkUtils::NetworkAddress NetworkUtils::GetInterfaceAddress(std::string_view interface)
{
    NetworkAddress address;

    struct ifaddrs* ifAddrStruct = nullptr;
    struct ifaddrs* ifa = nullptr;
    void* tmpAddrPtr = nullptr;

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }

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

    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }

    return address;
}

//------------------------------------------------------------------------------------------------

inline NetworkUtils::AddressComponentPair NetworkUtils::SplitAddressString(std::string_view str)
{
    if (str.empty()) {
        return {};
    }

    std::pair<std::string, std::string> components;

    auto const seperatorPosition = str.find_last_of(ComponentSeperator);
    // Get the primary endpoint component up until the seperator
    components.first = str.substr(0, seperatorPosition);
    // Get the secondary endpoint component skipping the seperator
    components.second = str.substr(seperatorPosition + 1);

    return components;
}

//------------------------------------------------------------------------------------------------
