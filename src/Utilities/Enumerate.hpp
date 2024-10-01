//----------------------------------------------------------------------------------------------------------------------
// File: Enumerate.hpp
// Description: A utility class for performing an enumerate method on containers. 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <iostream>
#include <iterator>
#include <utility>
#include <vector>
//----------------------------------------------------------------------------------------------------------------------

template<typename ContainerType>
concept IterableContainer = requires(ContainerType container) {
    typename ContainerType::iterator;
    std::begin(container);
    std::end(container);
};

//----------------------------------------------------------------------------------------------------------------------

template<typename ContainerType> requires IterableContainer<ContainerType>
class Enumerator
{
public:
    class iterator
    {
    public:
        using value_type = std::pair<std::size_t, typename ContainerType::const_reference>;
        using pointer = value_type*;
        using const_pointer = value_type const*;
        using reference = value_type&;
        using const_reference = value_type const&;
        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;

        iterator(std::size_t index, typename ContainerType::const_iterator itr)
            : m_index(index)
            , m_itr(itr)
        {
        }

        iterator& operator++()
        {
            ++m_index;
            ++m_itr;
            return *this;
        }

        iterator operator++(std::int32_t)
        {
            iterator result(*this);
            operator++();
            return result;
        }

        [[nodiscard]] reference operator*()
        {
            m_optValue.emplace(std::make_pair(m_index, std::cref(*m_itr)));
            return m_optValue.value();
        }

        [[nodiscard]] pointer operator->() { return &operator*(); }

        [[nodiscard]] std::strong_ordering operator<=>(iterator const& other) { return m_itr <=> other.m_itr; }
        [[nodiscard]] bool operator==(iterator const& other) const { return m_itr == other.m_itr; }

    private:
        std::size_t m_index;
        typename ContainerType::const_iterator m_itr;
        std::optional<value_type> m_optValue;
    };

    Enumerator(ContainerType& container)
        : m_container(container)
    {
    }

    [[nodiscard]] iterator begin() { return iterator(0, m_container.begin()); }
    [[nodiscard]] iterator end() { return iterator(0, m_container.end()); }

private:
    ContainerType& m_container;
};

//----------------------------------------------------------------------------------------------------------------------

template<typename ContainerType> requires IterableContainer<ContainerType>
Enumerator<ContainerType> enumerate(ContainerType& container) {
    return Enumerator<ContainerType>(container);
}

//----------------------------------------------------------------------------------------------------------------------
