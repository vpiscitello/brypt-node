//------------------------------------------------------------------------------------------------
// File: EndpointConstants.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <chrono>
#include <cstdint>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Network::Endpoint {
//------------------------------------------------------------------------------------------------

// The number of connections attempts allowed.
constexpr std::uint32_t ConnectRetryThreshold = 3;
// The number of outgoing messages to be sent in a cycle.
constexpr std::uint32_t EventProcessingLimit = 10;
// The number of times a message may be attempted to be sent.
constexpr std::uint32_t MessageRetryLimit = 3;

// The peroid of time between connection attemps.
constexpr std::chrono::seconds ConnectRetryTimeout = std::chrono::seconds(5);
// The minimum period before starting the next cycle.
constexpr std::chrono::nanoseconds CycleTimeout = std::chrono::nanoseconds(1000); 

//------------------------------------------------------------------------------------------------
} // Network::Endpoint namespace
//------------------------------------------------------------------------------------------------
