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

enum class Protocol : std::uint32_t { LoRa, TCP, Test, Invalid };

constexpr std::string_view TestScheme = "test";

static std::unordered_map<std::string, Protocol> const ProtocolStringTranslations = {
    { "lora", Protocol::LoRa },
    { "tcp", Protocol::TCP },
    { std::string{ TestScheme }, Protocol::Test },
};

static std::unordered_map<Protocol, std::string> const ProtocolEnumTranslations = {
    { Protocol::LoRa, "lora" },
    { Protocol::TCP, "tcp" },
    { Protocol::Test,  std::string{ TestScheme } },
};

using ProtocolSet = std::set<Network::Protocol>;

Protocol ParseProtocol(std::string name);
std::string ProtocolToString(Protocol protocol);

//----------------------------------------------------------------------------------------------------------------------
} // Network namespace
//----------------------------------------------------------------------------------------------------------------------

inline Network::Protocol Network::ParseProtocol(std::string name)
{
    // Adjust the provided protocol name to lowercase
    std::transform(name.begin(), name.end(), name.begin(), [] (char c) { 
        return static_cast<char>(std::tolower(static_cast<std::int32_t>(c)));
    });

    if (auto const itr = ProtocolStringTranslations.find(name.data()); itr != ProtocolStringTranslations.end()) {
        return itr->second;
    }

    return Protocol::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

inline std::string Network::ProtocolToString(Protocol protocol)
{
    if(auto const itr = ProtocolEnumTranslations.find(protocol); itr != ProtocolEnumTranslations.end()) {
        return itr->second;
    }
    
    return {};
}

//----------------------------------------------------------------------------------------------------------------------
