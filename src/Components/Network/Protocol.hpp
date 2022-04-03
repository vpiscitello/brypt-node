//----------------------------------------------------------------------------------------------------------------------
// File: Protocol.hpp
// Description: Defines an enum describing the types of network protocols available.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <cstdint>
#include <string>
#include <set>
#include <unordered_map>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Network {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::string_view TestScheme = "test";

enum class Protocol : std::uint8_t { LoRa, TCP, Test, Invalid };

using ProtocolSet = std::set<Network::Protocol>;

Protocol ParseProtocol(std::string name);
std::string ProtocolToString(Protocol protocol);

//----------------------------------------------------------------------------------------------------------------------
} // Network namespace
//----------------------------------------------------------------------------------------------------------------------

inline Network::Protocol Network::ParseProtocol(std::string name)
{
    static std::unordered_map<std::string, Protocol> const translations = {
        { "lora", Protocol::LoRa },
        { "tcp", Protocol::TCP },
        { std::string{ TestScheme }, Protocol::Test },
    };

    // Adjust the provided protocol name to lowercase
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c){ return std::tolower(c); });
    if (auto const itr = translations.find(name.data()); itr != translations.end()) {
        return itr->second;
    }
    return Protocol::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::string Network::ProtocolToString(Protocol protocol)
{
    static std::unordered_map<Protocol, std::string> const translations = {
        { Protocol::LoRa, "lora" },
        { Protocol::TCP, "tcp" },
        { Protocol::Test,  std::string{ TestScheme } },
    };

    if(auto const itr = translations.find(protocol); itr != translations.end()) {
        return itr->second;
    }
    return {};
}

//----------------------------------------------------------------------------------------------------------------------
