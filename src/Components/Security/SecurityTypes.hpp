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
#include <utility>
#include <vector>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Security {
//------------------------------------------------------------------------------------------------

using Buffer = std::vector<std::uint8_t>;
using OptionalBuffer = std::optional<Buffer>;
using SynchronizationResult = std::pair<SynchronizationStatus, Buffer>;

using Encryptor = std::function<OptionalBuffer(
    Buffer const& buffer, std::uint32_t size, std::uint64_t nonce)>;
using Decryptor = std::function<OptionalBuffer(
    Buffer const& buffer, std::uint32_t size, std::uint64_t nonce)>;
using Signator = std::function<std::int32_t(Buffer& buffer)>;
using Verifier = std::function<VerificationStatus(Buffer const& buffer)>;
using SignatureSizeGetter = std::function<std::int32_t()>;

//------------------------------------------------------------------------------------------------
} // Security namespace
//------------------------------------------------------------------------------------------------
