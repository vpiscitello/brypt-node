//----------------------------------------------------------------------------------------------------------------------
// File: StrongType.hpp
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <compare>
#include <concepts>
#include <cstdint>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

template<std::integral WeakType, typename TypeTag>
class StrongType
{
public:
    using UnderlyingType = WeakType;

    StrongType() = default;
    ~StrongType() = default;

    constexpr StrongType(StrongType<WeakType, TypeTag> const& other) : m_value(other.m_value) {}
    constexpr StrongType(StrongType<WeakType, TypeTag>&& other) : m_value(std::move(other.m_value)) { }
    constexpr explicit StrongType(UnderlyingType const& value) : m_value(value) {}
    constexpr explicit StrongType(UnderlyingType&& value) : m_value(std::move(value)) { }

    constexpr bool operator==(StrongType<WeakType, TypeTag> const& other) const 
    { 
        return m_value == other.m_value; 
    }

    constexpr std::strong_ordering operator<=>(StrongType<WeakType, TypeTag> const& other) const
    {
        return m_value <=> other.m_value;
    }

    constexpr StrongType<WeakType, TypeTag>& operator=(StrongType<WeakType, TypeTag> const& other)
    {
        m_value = other.m_value;
        return *this;
    }
    
    constexpr StrongType<WeakType, TypeTag>& operator=(StrongType<WeakType, TypeTag>&& other)
    {
        m_value = std::move(other.m_value);
        return *this;
    }
    
    constexpr StrongType<WeakType, TypeTag> operator+(StrongType<WeakType, TypeTag> const& other) const
    {
        return StrongType<WeakType, TypeTag>{ m_value + other.m_value };
    }
    
    constexpr StrongType<WeakType, TypeTag> operator-(StrongType<WeakType, TypeTag> const& other) const
    {
        return StrongType<WeakType, TypeTag>{ m_value - other.m_value };
    }
    
    constexpr StrongType<WeakType, TypeTag> operator*(StrongType<WeakType, TypeTag> const& other) const 
    {
        return StrongType<WeakType, TypeTag>{ m_value *= other.m_value };
    }
    
    constexpr StrongType<WeakType, TypeTag> operator/(StrongType<WeakType, TypeTag> const& other) const
    {
        return StrongType<WeakType, TypeTag>{ m_value / other.m_value };
    }
    
    constexpr StrongType<WeakType, TypeTag> operator%(StrongType<WeakType, TypeTag> const& other) const
    {
        return StrongType<WeakType, TypeTag>{ m_value % other.m_value };
    }

    constexpr StrongType<WeakType, TypeTag>& operator+=(StrongType<WeakType, TypeTag> const& other)
    {
        m_value += other.m_value;
        return *this;
    }
    
    constexpr StrongType<WeakType, TypeTag>& operator-=(StrongType<WeakType, TypeTag> const& other)
    {
        m_value -= other.m_value;
        return *this;
    }
    
    constexpr StrongType<WeakType, TypeTag>& operator*=(StrongType<WeakType, TypeTag> const& other)
    {
        m_value *= other.m_value;
        return *this;
    }
    
    constexpr StrongType<WeakType, TypeTag>& operator/=(StrongType<WeakType, TypeTag> const& other)
    {
        m_value /= other.m_value;
        return *this;
    }
    
    constexpr StrongType<WeakType, TypeTag>& operator%=(StrongType<WeakType, TypeTag> const& other)
    {
        m_value %= other.m_value;
        return *this;
    }

    constexpr StrongType<WeakType, TypeTag>& operator++()
    {
        ++m_value;
        return *this;
    }
    
    constexpr StrongType<WeakType, TypeTag> operator++(std::int32_t)
    {
        return StrongType<WeakType, TypeTag>{ m_value++ };
    }
    
    constexpr StrongType<WeakType, TypeTag>& operator--()
    {
        --m_value;
        return *this;
    }
    
    constexpr StrongType<WeakType, TypeTag> operator--(std::int32_t)
    {
        return StrongType<WeakType, TypeTag>{ m_value-- };
    }

    constexpr UnderlyingType& GetValue() { return m_value; }
    constexpr UnderlyingType const& GetValue() const { return m_value; }

private:
    UnderlyingType m_value;
};

//----------------------------------------------------------------------------------------------------------------------
