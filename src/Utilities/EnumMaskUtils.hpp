
// File: EnumMaskUtils.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <type_traits>
//------------------------------------------------------------------------------------------------

template <typename UnderlyingType>
constexpr UnderlyingType MaskLevel(UnderlyingType exponent) {
    return (UnderlyingType(1) << exponent);
}

//------------------------------------------------------------------------------------------------

template<typename EnumType> struct EnableEnumMaskOperators { static const bool enabled = false; };

//------------------------------------------------------------------------------------------------

#define ENABLE_ENUM_MASKING(EnumType) template<> struct EnableEnumMaskOperators<EnumType> \
{ static const bool enabled = true; };

//------------------------------------------------------------------------------------------------

template <typename EnumType>
typename std::enable_if_t<EnableEnumMaskOperators<EnumType>::enabled, EnumType>
operator| (EnumType left, EnumType right)
{
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    return static_cast<EnumType> (
        static_cast<UnderlyingType>(left) | static_cast<UnderlyingType>(right)
    );
}

//------------------------------------------------------------------------------------------------

template <typename EnumType>
typename std::enable_if_t<EnableEnumMaskOperators<EnumType>::enabled, EnumType>
operator& (EnumType left, EnumType right)
{
    using UnderlyingType = typename std::underlying_type<EnumType>::type;
    return static_cast<EnumType> (
        static_cast<UnderlyingType>(left) & static_cast<UnderlyingType>(right)
    );
}

//------------------------------------------------------------------------------------------------

template <typename EnumType,
          typename std::enable_if_t<EnableEnumMaskOperators<EnumType>::enabled, EnumType>* = nullptr>
bool FlagIsSet(EnumType flag, EnumType mask)
{
    if (auto const result = (flag & mask); result == flag) {
        return true;
    }
    return false;
}

//------------------------------------------------------------------------------------------------

template <typename EnumType,
          typename MaskContainerType,
          typename std::enable_if_t<EnableEnumMaskOperators<EnumType>::enabled, EnumType>* = nullptr,
          typename std::enable_if_t<std::is_array_v<MaskContainerType>>* = nullptr>
bool FlagIncluded(EnumType flag, MaskContainerType const& masks)
{
    for (auto const& mask : masks) {
        if (FlagIsSet(flag, mask)) {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------------------------
