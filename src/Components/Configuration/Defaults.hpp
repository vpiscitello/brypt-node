
//----------------------------------------------------------------------------------------------------------------------
// File: Parser.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Configuration::Defaults {
//----------------------------------------------------------------------------------------------------------------------

constexpr std::uint32_t FileSizeLimit = 12'000; // Limit the configuration files to 12KB

#if !defined(WIN32)
std::filesystem::path const FallbackConfigurationFolder = "/etc/";
#endif

constexpr std::string_view IdentifierType = "Persistent";

constexpr std::string_view EndpointType = "TCP";
constexpr std::string_view NetworkInterface = "lo";
constexpr std::string_view TcpBindingAddress = "*:35216";
constexpr std::string_view TcpBootstrapEntry = "127.0.0.1:35216";
constexpr std::string_view LoRaBindingAddress = "915:71";

constexpr auto ConnectionTimeout = std::chrono::milliseconds{ 15'000 };
constexpr std::uint32_t ConnectionRetryLimit = 3;
constexpr auto ConnectionRetryInterval = std::chrono::milliseconds{ 5'000 }; 

constexpr std::string_view NetworkToken = "";

//----------------------------------------------------------------------------------------------------------------------
} // Configuration::Defaults namespace
//----------------------------------------------------------------------------------------------------------------------
