//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
//------------------------------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
//------------------------------------------------------------------------------------------------

namespace Command {
    class IHandler;
}

class CConnection;

//------------------------------------------------------------------------------------------------
namespace NodeUtils {
//------------------------------------------------------------------------------------------------

using NodeIdType = std::uint32_t;
using ClusterIdType = std::uint32_t;
using NetworkAddress = std::string;
using PortNumber = std::string;
using SerialNumber = std::string;

using AddressComponentPair = std::pair<std::string, std::string>;

using NetworkNonce = std::uint32_t;
using TimePoint = std::chrono::system_clock::time_point;
using TimePeriod = std::chrono::milliseconds;

using ObjectIdType = std::uint32_t;

//------------------------------------------------------------------------------------------------

enum class DeviceOperation : std::uint8_t { Root, Branch, Leaf, None };
enum class ConnectionOperation : std::uint8_t { Server, Client, None };
enum class DeviceSocketCapability : std::uint8_t { Master, Slave };
enum class TechnologyType : std::uint8_t { Direct, LoRa, StreamBridge, TCP, None };
enum class NotificationType : std::uint8_t { Network, Cluster, Node, None };
enum class PrintType : std::uint8_t { Await, Command, Connection, Control, Message, MessageQueue, Node, Notifier, PeerWatcher, Error };

//------------------------------------------------------------------------------------------------

using ConnectionMap = std::unordered_map<NodeIdType, std::shared_ptr<CConnection>>;

//------------------------------------------------------------------------------------------------

constexpr std::string_view NodeVersion = "0.0.0-alpha";
constexpr std::string_view NetworkKey = "01234567890123456789012345678901";

constexpr std::uint32_t PORT_GAP = 16;

constexpr char const* ADDRESS_COMPONENT_SEPERATOR = ":";
constexpr char const* ID_SEPERATOR = ":";

NodeIdType GenerateNetworkId();

TechnologyType ParseTechnologyType(std::string name);
std::string TechnologyTypeToString(TechnologyType technology);

std::string GetDesignation(DeviceOperation const& operation);

TimePoint GetSystemTimePoint();
std::string GetSystemTimestamp();
std::string TimePointToString(TimePoint const& time);
NodeUtils::TimePeriod TimePointToTimePeriod(TimePoint const& time);
TimePoint StringToTimePoint(std::string const& timestamp);

std::string GetPrintEscape(PrintType const& component);

AddressComponentPair SplitAddressString(std::string_view str);

NetworkAddress GetLocalAddress(std::string_view interface);

void printo(std::string_view message, PrintType component);

//------------------------------------------------------------------------------------------------
} // NodeUtils namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

inline NodeUtils::NodeIdType NodeUtils::GenerateNetworkId()
{
    return 0;
}

//------------------------------------------------------------------------------------------------

inline NodeUtils::TechnologyType NodeUtils::ParseTechnologyType(std::string name)
{
    static std::unordered_map<std::string, TechnologyType> const technologyMap = {
        {"direct", TechnologyType::Direct},
        {"lora", TechnologyType::LoRa},
        {"streambridge", TechnologyType::StreamBridge},
        {"tcp", TechnologyType::TCP},
    };

    // Adjust the provided technology name to lowercase
    std::transform(name.begin(), name.end(), name.begin(),
    [](unsigned char c){
        return std::tolower(c);
    });

    if(auto const itr = technologyMap.find(name); itr != technologyMap.end()) {
        return itr->second;
    }
    return TechnologyType::None;
}

//------------------------------------------------------------------------------------------------

inline std::string NodeUtils::TechnologyTypeToString(TechnologyType technology)
{
    static std::unordered_map<TechnologyType, std::string> const technologyMap = {
        {TechnologyType::Direct, "Direct"},
        {TechnologyType::LoRa, "LoRa"},
        {TechnologyType::StreamBridge, "StreamBridge"},
        {TechnologyType::TCP, "TCP"},
    };

    if(auto const itr = technologyMap.find(technology); itr != technologyMap.end()) {
        return itr->second;
    }
    return {};
}

//------------------------------------------------------------------------------------------------

inline std::string NodeUtils::GetDesignation(DeviceOperation const& operation)
{
    static std::unordered_map<DeviceOperation, std::string> const designationMap = {
        {DeviceOperation::Root, "root"},
        {DeviceOperation::Branch, "coordinator"},
        {DeviceOperation::Leaf, "node"},
    };

    if(auto const itr = designationMap.find(operation); itr != designationMap.end()) {
        return itr->second;
    }
    return std::string();
}

//------------------------------------------------------------------------------------------------

inline NodeUtils::TimePoint NodeUtils::GetSystemTimePoint()
{
    return std::chrono::system_clock::now();
}

//------------------------------------------------------------------------------------------------

inline std::string NodeUtils::GetSystemTimestamp()
{
    TimePoint const current = GetSystemTimePoint();
    auto const milliseconds = std::chrono::duration_cast<TimePeriod>(current.time_since_epoch());

    std::stringstream epochStream;
    epochStream.clear();
    epochStream << milliseconds.count();
    return epochStream.str();
}

//------------------------------------------------------------------------------------------------

inline std::string NodeUtils::TimePointToString(TimePoint const& time)
{
    auto const milliseconds = TimePointToTimePeriod(time);

    std::stringstream epochStream;
    epochStream.clear();
    epochStream << milliseconds.count();
    return epochStream.str();
}

//------------------------------------------------------------------------------------------------

inline NodeUtils::TimePeriod NodeUtils::TimePointToTimePeriod(TimePoint const& time)
{
    return std::chrono::duration_cast<TimePeriod>(time.time_since_epoch());
}

//------------------------------------------------------------------------------------------------

inline NodeUtils::TimePoint NodeUtils::StringToTimePoint(std::string const& timestamp)
{
    std::int64_t const llMilliseconds = std::stoll(timestamp);
    TimePeriod const milliseconds(llMilliseconds);
    return std::chrono::system_clock::time_point(milliseconds);
}

//------------------------------------------------------------------------------------------------

inline std::string NodeUtils::GetPrintEscape(PrintType const& component)
{
    static std::unordered_map<PrintType, std::string> const escapeMap = {
        {PrintType::Await, "\x1b[1;30;48;5;93m[    Await    ]\x1b[0m "},
        {PrintType::Command, "\x1b[1;30;48;5;220m[   Command   ]\x1b[0m "},
        {PrintType::Connection, "\x1b[1;30;48;5;6m[  Connection ]\x1b[0m "},
        {PrintType::Control, "\x1b[1;97;48;5;4m[   Control   ]\x1b[0m "},
        {PrintType::Message, "\x1b[1;30;48;5;135m[   Message   ]\x1b[0m "},
        {PrintType::MessageQueue, "\x1b[1;30;48;5;129m[ MessageQueue ]\x1b[0m "},
        {PrintType::Node, "\x1b[1;30;48;5;42m[     Node    ]\x1b[0m "},
        {PrintType::Notifier, "\x1b[1;30;48;5;12m[   Notifier  ]\x1b[0m "},
        {PrintType::PeerWatcher, "\x1b[1;30;48;5;203m[ PeerWatcher ]\x1b[0m "},
        {PrintType::Error, "\x1b[1;30;48;5;196m[    ERROR    ]\x1b[0m "},
    };

    if(auto const itr = escapeMap.find(component); itr != escapeMap.end()) {
        return itr->second;
    }
    return std::string();
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------------------------
inline NodeUtils::NetworkAddress NodeUtils::GetLocalAddress(std::string_view interface)
{
    NetworkAddress ip = std::string();

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
                ip = std::string(addressBuffer);
                break;
            }
        }
    }

    if (ifAddrStruct != nullptr) {
        freeifaddrs(ifAddrStruct);
    }

    return ip;
}

//------------------------------------------------------------------------------------------------

inline NodeUtils::AddressComponentPair NodeUtils::SplitAddressString(std::string_view str)
{
    if (str.empty()) {
        return {};
    }

    std::pair<std::string, std::string> components;

    auto const seperatorPosition = str.find_last_of(ADDRESS_COMPONENT_SEPERATOR);
    // Get the primary connection component up until the seperator
    components.first = str.substr(0, seperatorPosition);
    // Get the secondary connection component skipping the seperator
    components.second = str.substr(seperatorPosition + 1);

    return components;
}

//------------------------------------------------------------------------------------------------

inline void NodeUtils::printo(std::string_view message, PrintType component)
{
	std::string const escape = GetPrintEscape(component);
	std::cout << "== " << escape << message << std::endl << std::flush;
}

//------------------------------------------------------------------------------------------------
