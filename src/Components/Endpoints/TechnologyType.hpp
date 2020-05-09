//------------------------------------------------------------------------------------------------
// File: TechnologyType.hpp
// Description: Defines an enum describing the types of Endpoints avaulable
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <string>
#include <set>
#include <unordered_map>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Endpoints {
//------------------------------------------------------------------------------------------------

enum class TechnologyType : std::uint8_t { Direct, LoRa, StreamBridge, TCP, Invalid };

using TechnologySet = std::set<Endpoints::TechnologyType>;

TechnologyType ParseTechnologyType(std::string name);
std::string TechnologyTypeToString(TechnologyType technology);

//------------------------------------------------------------------------------------------------
} // Endpoint namespace
//------------------------------------------------------------------------------------------------

inline Endpoints::TechnologyType Endpoints::ParseTechnologyType(std::string name)
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

    if(auto const itr = technologyMap.find(name.data()); itr != technologyMap.end()) {
        return itr->second;
    }
    return TechnologyType::Invalid;
}

//------------------------------------------------------------------------------------------------

inline std::string Endpoints::TechnologyTypeToString(TechnologyType technology)
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
