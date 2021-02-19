//------------------------------------------------------------------------------------------------
// File: SecurityTypes.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "SecurityDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Security {
//------------------------------------------------------------------------------------------------

using Buffer = std::vector<std::uint8_t>;
using ReadableView = std::span<std::uint8_t const, std::dynamic_extent>;
using OptionalBuffer = std::optional<Buffer>;
using SynchronizationResult = std::pair<SynchronizationStatus, Buffer>;

using Encryptor = std::function<OptionalBuffer(ReadableView buffer, std::uint64_t nonce)>;
using Decryptor = std::function<OptionalBuffer(ReadableView buffer, std::uint64_t nonce)>;
using Signator = std::function<std::int32_t(Buffer& buffer)>;
using Verifier = std::function<VerificationStatus(ReadableView buffer)>;
using SignatureSizeGetter = std::function<std::size_t()>;

//------------------------------------------------------------------------------------------------
} // Security namespace
//------------------------------------------------------------------------------------------------
