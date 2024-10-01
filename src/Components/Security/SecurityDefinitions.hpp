//----------------------------------------------------------------------------------------------------------------------
// File: SecurityDefinitions.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <map>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

enum class ConfidentialityLevel : std::uint32_t { Unknown, Low, Medium, High };

enum class ExchangeRole : std::uint32_t { Initiator, Acceptor };

enum class SynchronizationStatus : std::uint32_t { Error, Processing, Ready };

enum class VerificationStatus : std::uint32_t { Failed, Success };

constexpr std::size_t SupportedConfidentialityLevelSize = 3;
constexpr std::size_t MaximumSupportedAlgorithmElements = 16;
constexpr std::size_t MaximumSupportedAlgorithmNameSize = 128;
constexpr std::size_t MaximumExpectedPublicKeySize = 512'000;
constexpr std::size_t MaximumExpectedSaltSize = 8'192;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------
