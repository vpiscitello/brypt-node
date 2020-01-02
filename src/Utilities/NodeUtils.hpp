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
    class CHandler;
}

class CConnection;

//------------------------------------------------------------------------------------------------
namespace NodeUtils {
//------------------------------------------------------------------------------------------------

using NodeIdType = std::uint32_t;
using ClusterIdType = std::uint32_t;
using IPv4Address = std::string;
using IPv6Address = std::string;
using PortNumber = std::string;
using SerialNumber = std::string;

using AddressComponentPair = std::pair<std::string, std::string>;

using NetworkKey = std::string;
using NetworkNonce = std::uint32_t;
using TimePoint = std::chrono::system_clock::time_point;
using TimePeriod = std::chrono::milliseconds;

using ObjectIdType = std::uint32_t;

//------------------------------------------------------------------------------------------------

enum class DeviceOperation : std::uint8_t { ROOT, BRANCH, LEAF, NONE };
enum class DeviceSocketCapability : std::uint8_t { MASTER, SLAVE };
enum class TechnologyType : std::uint8_t { DIRECT, LORA, STREAMBRIDGE, TCP, NONE };
enum class CommandType : std::uint8_t { INFORMATION, QUERY, ELECTION, TRANSFORM, CONNECT, NONE };
enum class NotificationType : std::uint8_t { NETWORK, CLUSTER, NODE, NONE };
enum class PrintType : std::uint8_t { AWAIT, COMMAND, CONNECTION, CONTROL, MESSAGE, MQUEUE, NODE, NOTIFIER, WATCHER, ERROR };

//------------------------------------------------------------------------------------------------

using CommandMap = std::unordered_map<CommandType, std::unique_ptr<Command::CHandler>>;
using ConnectionMap = std::unordered_map<NodeIdType, std::shared_ptr<CConnection>>;

//------------------------------------------------------------------------------------------------

constexpr std::string_view NODE_VERSION = "0.0.0-alpha";
constexpr std::string_view NETWORK_KEY = "01234567890123456789012345678901";

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

IPv4Address GetLocalAddress(std::string_view interface);

void printo(std::string const& message, PrintType component);

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
        {"direct", TechnologyType::DIRECT},
        {"lora", TechnologyType::LORA},
        {"streambridge", TechnologyType::STREAMBRIDGE},
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
    return TechnologyType::NONE;
}

//------------------------------------------------------------------------------------------------

inline std::string NodeUtils::TechnologyTypeToString(TechnologyType technology)
{
    static std::unordered_map<TechnologyType, std::string> const technologyMap = {
        {TechnologyType::DIRECT, "Direct"},
        {TechnologyType::LORA, "LoRa"},
        {TechnologyType::STREAMBRIDGE, "StreamBridge"},
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
        {DeviceOperation::ROOT, "root"},
        {DeviceOperation::BRANCH, "coordinator"},
        {DeviceOperation::LEAF, "node"},
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
        {PrintType::AWAIT, "\x1b[1;30;48;5;93m[    Await    ]\x1b[0m "},
        {PrintType::COMMAND, "\x1b[1;30;48;5;220m[   Command   ]\x1b[0m "},
        {PrintType::CONNECTION, "\x1b[1;30;48;5;6m[  Connection ]\x1b[0m "},
        {PrintType::CONTROL, "\x1b[1;97;48;5;4m[   Control   ]\x1b[0m "},
        {PrintType::MESSAGE, "\x1b[1;30;48;5;135m[   Message   ]\x1b[0m "},
        {PrintType::MQUEUE, "\x1b[1;30;48;5;129m[ MessageQueue ]\x1b[0m "},
        {PrintType::NODE, "\x1b[1;30;48;5;42m[     Node    ]\x1b[0m "},
        {PrintType::NOTIFIER, "\x1b[1;30;48;5;12m[   Notifier  ]\x1b[0m "},
        {PrintType::WATCHER, "\x1b[1;30;48;5;203m[ PeerWatcher ]\x1b[0m "},
        {PrintType::ERROR, "\x1b[1;30;48;5;196m[    ERROR    ]\x1b[0m "},
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
inline NodeUtils::IPv4Address NodeUtils::GetLocalAddress(std::string_view interface)
{
    IPv4Address ip = std::string();

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

inline void NodeUtils::printo(std::string const& message, PrintType component)
{
	std::string const escape = GetPrintEscape(component);
	std::cout << "== " << escape << message << std::endl;
}

//------------------------------------------------------------------------------------------------
