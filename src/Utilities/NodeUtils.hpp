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

//------------------------------------------------------------------------------------------------

enum class DeviceOperation : std::uint8_t { Branch, Leaf, None };
enum class NotificationType : std::uint8_t { Network, Cluster, Node, None };
enum class PrintType : std::uint8_t { Await, Handler, Endpoint, Message, MessageControl, Node, Error };

//------------------------------------------------------------------------------------------------

constexpr std::string_view NetworkKey = "01234567890123456789012345678901";

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
    static auto const* const EscapeMappings = 
        new std::unordered_map<PrintType, std::string> {
            {PrintType::Await, "\x1b[1;30;48;5;93m[    Await    ]\x1b[0m "},
            {PrintType::Handler, "\x1b[1;30;48;5;220m[   Handler   ]\x1b[0m "},
            {PrintType::Endpoint, "\x1b[1;30;48;5;6m[   Endpoint  ]\x1b[0m "},
            {PrintType::Message, "\x1b[1;30;48;5;135m[   Message   ]\x1b[0m "},
            {PrintType::MessageControl, "\x1b[1;30;48;5;129m[ MessageControl ]\x1b[0m "},
            {PrintType::Node, "\x1b[1;30;48;5;42m[     Node    ]\x1b[0m "},
            {PrintType::Error, "\x1b[1;30;48;5;196m[    Error    ]\x1b[0m "}, };

    if(auto const itr = EscapeMappings->find(component); itr != EscapeMappings->end()) {
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
