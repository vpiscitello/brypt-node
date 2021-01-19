//------------------------------------------------------------------------------------------------
// File: Protocol.hpp
// Description: Defines an enum describing the types of network protocols available.
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
namespace Network {
//------------------------------------------------------------------------------------------------

enum class Protocol : std::uint8_t { LoRa, TCP, Invalid };

using ProtocolSet = std::set<Network::Protocol>;

Protocol ParseProtocol(std::string name);
std::string ProtocolToString(Protocol protocol);

//------------------------------------------------------------------------------------------------
} // Endpoint namespace
//------------------------------------------------------------------------------------------------

inline Network::Protocol Network::ParseProtocol(std::string name)
{
    static std::unordered_map<std::string, Protocol> const protocolMap = {
        {"lora", Protocol::LoRa},
        {"tcp", Protocol::TCP},
    };

    // Adjust the provided protocol name to lowercase
    std::transform(name.begin(), name.end(), name.begin(),
    [](unsigned char c){
        return std::tolower(c);
    });

    if(auto const itr = protocolMap.find(name.data()); itr != protocolMap.end()) {
        return itr->second;
    }
    return Protocol::Invalid;
}

//------------------------------------------------------------------------------------------------

inline std::string Network::ProtocolToString(Protocol protocol)
{
    static std::unordered_map<Protocol, std::string> const protocolMap = {
        {Protocol::LoRa, "LoRa"},
        {Protocol::TCP, "TCP"},
    };

    if(auto const itr = protocolMap.find(protocol); itr != protocolMap.end()) {
        return itr->second;
    }
    return {};
}

//------------------------------------------------------------------------------------------------
