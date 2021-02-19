//------------------------------------------------------------------------------------------------
// File: SecureBuffer.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "SecureBuffer.hpp"
#include "SecurityUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
#include <iostream>
//------------------------------------------------------------------------------------------------

Security::SecureBuffer::SecureBuffer(Security::Buffer&& buffer)
    : m_buffer(std::move(buffer))
{
}

//------------------------------------------------------------------------------------------------

Security::SecureBuffer::~SecureBuffer()
{
    Security::EraseMemory(m_buffer.data(), m_buffer.size());
}

//------------------------------------------------------------------------------------------------

Security::ReadableView Security::SecureBuffer::GetData() const
{
    return m_buffer;
}

//------------------------------------------------------------------------------------------------

Security::ReadableView Security::SecureBuffer::GetCordon(
    std::size_t offset, std::size_t size) const
{
    assert(offset + size <= m_buffer.size());
    auto const begin = m_buffer.data() + offset;
    auto const end = begin + size;
    return ReadableView(begin, end);
}

//------------------------------------------------------------------------------------------------

std::size_t Security::SecureBuffer::GetSize() const
{
    return m_buffer.size();
}

//------------------------------------------------------------------------------------------------
