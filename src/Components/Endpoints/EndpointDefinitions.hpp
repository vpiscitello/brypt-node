//------------------------------------------------------------------------------------------------
// File: EndpointConstants.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <chrono>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Endpoint {
//------------------------------------------------------------------------------------------------

constexpr std::uint32_t ConnectRetryThreshold = 3; // The number of connections attempts allowed
constexpr std::uint32_t OutgoingMessageLimit = 10; // The number of outgoing messages to be sent in a cycle
constexpr std::uint32_t MessageRetryLimit = 3; // The number of times a message may be attempted to be sent

constexpr std::chrono::seconds ConnectRetryTimeout = std::chrono::seconds(5); // The peroid of time between connection attemps
constexpr std::chrono::nanoseconds CycleTimeout = std::chrono::nanoseconds(1000); // The minimum period before starting the next cycle

//------------------------------------------------------------------------------------------------
} // Endpoint namespace
//------------------------------------------------------------------------------------------------