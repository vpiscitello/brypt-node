//----------------------------------------------------------------------------------------------------------------------
// File: SecurityTypes.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "SecurityDefinitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

using Buffer = std::vector<std::uint8_t>;
using ReadableView = std::span<std::uint8_t const, std::dynamic_extent>;
using WriteableView = std::span<std::uint8_t, std::dynamic_extent>;
using OptionalBuffer = std::optional<Buffer>;
using SynchronizationResult = std::pair<SynchronizationStatus, Buffer>;

using Encryptor = std::function<bool(ReadableView, Buffer&)>;
using Decryptor = std::function<OptionalBuffer(ReadableView)>;
using EncryptedSizeGetter = std::function<std::size_t(std::size_t)>;
using Signator = std::function<bool(Buffer&)>;
using Verifier = std::function<VerificationStatus(ReadableView)>;
using SignatureSizeGetter = std::function<std::size_t()>;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------
