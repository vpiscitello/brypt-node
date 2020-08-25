//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace NodeUtils {
//------------------------------------------------------------------------------------------------

using ClusterIdType = std::uint32_t;

using NetworkNonce = std::uint32_t;

using ObjectIdType = std::uint32_t;

//------------------------------------------------------------------------------------------------

enum class DeviceOperation : std::uint8_t { Branch, Leaf, None };
enum class NotificationType : std::uint8_t { Network, Cluster, Node, None };
enum class PrintType : std::uint8_t { Await, Command, Control, Endpoint, Message, MessageQueue, Node, Notifier, PeerWatcher, Error };

//------------------------------------------------------------------------------------------------

constexpr std::string_view NetworkKey = "01234567890123456789012345678901";

constexpr char const* IDSeperator = ":";

std::string GetDesignation(DeviceOperation const& operation);

std::string GetPrintEscape(PrintType const& component);
void printo(std::string_view message, PrintType component);

//------------------------------------------------------------------------------------------------
} // NodeUtils namespace
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------

inline std::string NodeUtils::GetDesignation(DeviceOperation const& operation)
{
    static std::unordered_map<DeviceOperation, std::string> const designationMap = {
        {DeviceOperation::Branch, "coordinator"},
        {DeviceOperation::Leaf, "node"},
    };

    if(auto const itr = designationMap.find(operation); itr != designationMap.end()) {
        return itr->second;
    }

    return "";
}

//------------------------------------------------------------------------------------------------

inline std::string NodeUtils::GetPrintEscape(PrintType const& component)
{
    static std::unordered_map<PrintType, std::string> const escapeMap = {
        {PrintType::Await, "\x1b[1;30;48;5;93m[    Await    ]\x1b[0m "},
        {PrintType::Command, "\x1b[1;30;48;5;220m[   Command   ]\x1b[0m "},
        {PrintType::Control, "\x1b[1;97;48;5;4m[   Control   ]\x1b[0m "},
        {PrintType::Endpoint, "\x1b[1;30;48;5;6m[   Endpoint  ]\x1b[0m "},
        {PrintType::Message, "\x1b[1;30;48;5;135m[   Message   ]\x1b[0m "},
        {PrintType::MessageQueue, "\x1b[1;30;48;5;129m[ MessageQueue ]\x1b[0m "},
        {PrintType::Node, "\x1b[1;30;48;5;42m[     Node    ]\x1b[0m "},
        {PrintType::Notifier, "\x1b[1;30;48;5;12m[   Notifier  ]\x1b[0m "},
        {PrintType::PeerWatcher, "\x1b[1;30;48;5;203m[ PeerWatcher ]\x1b[0m "},
        {PrintType::Error, "\x1b[1;30;48;5;196m[    Error    ]\x1b[0m "},
    };

    if(auto const itr = escapeMap.find(component); itr != escapeMap.end()) {
        return itr->second;
    }
    return std::string();
}

//------------------------------------------------------------------------------------------------

inline void NodeUtils::printo(std::string_view message, PrintType component)
{
	std::string const escape = GetPrintEscape(component);
	std::cout << "== " << escape << message << std::endl << std::flush;
}

//------------------------------------------------------------------------------------------------
