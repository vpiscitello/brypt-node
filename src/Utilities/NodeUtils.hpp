//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace NodeUtils {
//------------------------------------------------------------------------------------------------

using ClusterIdType = std::uint32_t;

enum class DeviceOperation : std::uint8_t { Branch, Leaf, None };
enum class NotificationType : std::uint8_t { Network, Cluster, Node, None };

std::string GetDesignation(DeviceOperation const& operation);

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
