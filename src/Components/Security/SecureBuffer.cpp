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

Security::CSecureBuffer::CSecureBuffer(Security::Buffer&& buffer)
    : m_buffer(std::move(buffer))
{
}

//------------------------------------------------------------------------------------------------

Security::CSecureBuffer::~CSecureBuffer()
{
    Security::EraseMemory(m_buffer.data(), m_buffer.size());
}

//------------------------------------------------------------------------------------------------

Security::ReadableView Security::CSecureBuffer::GetData() const
{
    return m_buffer;
}

//------------------------------------------------------------------------------------------------

Security::ReadableView Security::CSecureBuffer::GetCordon(
    std::size_t offset, std::size_t size) const
{
    assert(offset + size <= m_buffer.size());
    auto const begin = m_buffer.begin() + offset;
    auto const end = begin + size;
    return ReadableView(begin, end);
}

//------------------------------------------------------------------------------------------------

std::size_t Security::CSecureBuffer::GetSize() const
{
    return m_buffer.size();
}

//------------------------------------------------------------------------------------------------
