//----------------------------------------------------------------------------------------------------------------------
// File: SerializationErrors.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <concepts>
#include <locale>
#include <string>
#include <string_view>
#include <type_traits>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Configuration {
//----------------------------------------------------------------------------------------------------------------------

inline [[nodiscard]] std::string GetIndefiniteArticle(std::string_view value)
{
    if (value.empty()) { return ""; }

    char const character = std::tolower(value.front(), std::locale(""));
    switch (character) {
        case 'a':
        case 'e':
        case 'i':
        case 'o':
        case 'u': return "an";
        default: return "a";
    }
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ValueType, std::size_t ArraySize>
std::string CreateArrayString(std::array<std::pair<std::string, ValueType>, ArraySize> const& values)
{
    if (values.size() > 5) {
        return "See documentation for supported values.";
    }

    std::ostringstream oss;
    oss << "Expected: ";

    if (values.size() == 1) {
        oss << values[0].first;
    } else if (values.size() == 2) {
        oss << values[0].first << " or " << values[1].first;
    } else {
        for (size_t idx = 0; idx < values.size(); ++idx) {
            if (idx > 0) {
                oss << "\", ";
            }
            if (idx == values.size() - 1) {
                oss << "or ";
            }
            oss << "\"" << values[idx].first;
        }
    }

    return oss.str();
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string ConcatenateFieldNames(Fields const&... fields)
{
    std::string result;
    ((result += std::string(result.empty() ? "" : ".").append(fields)), ...);
    return result;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateArrayContextString(std::size_t index, Fields const&... fields)
{
    std::string result;
    ((result += std::string(result.empty() ? "" : ".").append(fields)), ...);

    std::ostringstream oss;
    oss << "[" << index << "]";
    result.append(oss.str());

    return result;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateUnexpectedErrorMessage(Fields const&... fields)
{
    return std::format("Encountered an unexpected error handling the '{}' field.", ConcatenateFieldNames(fields...));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateMissingFieldMessage(Fields const&... fields)
{
    return std::format("The '{}' field was not found.", ConcatenateFieldNames(fields...));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateEmptyArrayFieldMessage(Fields const&... fields)
{
    return std::format("The '{}' field contained no valid elements.", ConcatenateFieldNames(fields...));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateExceededElementLinitMessage(std::integral auto max, Fields const&... fields)
{
    return std::format(
        "The '{}' field exceeded the maximum number of {} allowable elements.",
        max, ConcatenateFieldNames(fields...));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateMismatchedValueTypeMessage(std::string_view type, Fields const&... fields)
{
    return std::format(
        "The '{}' field must be {} {}.",
        ConcatenateFieldNames(fields...), GetIndefiniteArticle(type), type);
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
std::string CreateInvalidValueMessage(Fields const&... fields)
{
    return std::format(
        "The '{}' field contains an invalid value. See documentation for supported values.",
        ConcatenateFieldNames(fields...));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ValueType, std::size_t ArraySize, typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateUnexpectedFieldMessage(
    std::array<std::pair<std::string, ValueType>, ArraySize> const& values, Fields const&... fields)
{
    return std::format(
        "Encountered invalid field name at '{}'. {}",
        ConcatenateFieldNames(fields...), CreateArrayString(values));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ValueType, std::size_t ArraySize, typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateUnexpectedValueMessage(
    std::array<std::pair<std::string, ValueType>, ArraySize> const& values, Fields const&... fields)
{
    return std::format(
        "The '{}' field contains an invalid value. {}",
        ConcatenateFieldNames(fields...), CreateArrayString(values));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateInvalidValueInArrayMessage(std::string_view field, std::size_t index, Fields const&... arrayFields)
{
    return std::format(
        "The '{}' field in '{}' element of the '{}' array contains an invalid value. See documentation for supported values.",
        ConcatenateFieldNames(field, index, arrayFields...));
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateExceededValueLimitMessage(std::integral auto max, Fields const&... fields)
{
    return std::format(
        "The '{}' field must not exceed a value of '{}.", ConcatenateFieldNames(fields...), max);
}

//----------------------------------------------------------------------------------------------------------------------

template<typename... Fields> requires (std::convertible_to<Fields, std::string_view> && ...)
[[nodiscard]] std::string CreateExceededCharacterLimitMessage(std::integral auto max, Fields const&... fields)
{
    return std::format(
        "The '{}' field exceeds the maximum allowed length of '{}' characters.",
        ConcatenateFieldNames(fields...), max);
}

//----------------------------------------------------------------------------------------------------------------------
} // Configuration namespace
//----------------------------------------------------------------------------------------------------------------------
