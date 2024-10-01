//----------------------------------------------------------------------------------------------------------------------
// File: SecureBuffer.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "SecurityTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Security {
//----------------------------------------------------------------------------------------------------------------------

class SecureBuffer;

using OptionalSecureBuffer = std::optional<Security::SecureBuffer>;

template <typename Buffer>
concept ByteLikeBuffer = std::same_as<typename Buffer::value_type, std::uint8_t> ||
                         std::same_as<typename Buffer::value_type, char>;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::SecureBuffer
{
public:
    template <typename... Buffers>
    SecureBuffer(Buffers const&... buffers) requires (ByteLikeBuffer<Buffers> && ...)
    {
        Append(buffers...);
    }

    SecureBuffer(std::size_t size);

    SecureBuffer(SecureBuffer const& other) : m_buffer(other.m_buffer) {}

    SecureBuffer& operator=(SecureBuffer const& other) {
        m_buffer = other.m_buffer;
        return *this;
    }

    SecureBuffer(SecureBuffer&& other) noexcept : m_buffer(std::exchange(other.m_buffer, {})) {}

    SecureBuffer& operator=(SecureBuffer&& other) noexcept {
        if (this != &other) {
            m_buffer = std::exchange(other.m_buffer, {});
        }
        return *this;
    }

    ~SecureBuffer();

    [[nodiscard]] bool operator==(SecureBuffer const& other) const noexcept = default;
    [[nodiscard]] std::strong_ordering operator<=>(SecureBuffer const& other) const noexcept = default;

    template<typename T>
    auto Read(std::function<T(Buffer const&)> const& reader) const -> decltype(auto) { return std::invoke(reader, m_buffer); }

    [[nodiscard]] ReadableView GetData() const;
    [[nodiscard]] WriteableView GetData();
    [[nodiscard]] ReadableView GetCordon(std::size_t offset, std::size_t size) const;
    [[nodiscard]] std::size_t GetSize() const;
    [[nodiscard]] std::size_t IsEmpty() const;

    template <typename... Buffers>
    void Append(Buffers const&... buffers) requires (ByteLikeBuffer<Buffers> && ...)
    {
        if constexpr (sizeof...(Buffers) != 0) {
            // Calculate the total size of the concatenated buffers
            std::size_t const total = (buffers.size() + ...);

            // Reserve enough memory for the concatenated buffers
            m_buffer.reserve(total);

            // Concatenate the buffers into the internal buffer
            (m_buffer.insert(m_buffer.end(), buffers.begin(), buffers.end()), ...);
        }
    }

    void Resize(std::size_t size);
    void Erase();
    
private:
    Buffer m_buffer;
};

//----------------------------------------------------------------------------------------------------------------------
