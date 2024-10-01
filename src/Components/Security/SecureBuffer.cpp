//----------------------------------------------------------------------------------------------------------------------
// File: SecureBuffer.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "SecureBuffer.hpp"
#include "SecurityUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
#include <iostream>
//----------------------------------------------------------------------------------------------------------------------

Security::SecureBuffer::SecureBuffer(std::size_t size)
    : m_buffer(size)
{
}

//----------------------------------------------------------------------------------------------------------------------

Security::SecureBuffer::~SecureBuffer()
{
    EraseMemory(m_buffer.data(), m_buffer.size());
}

//----------------------------------------------------------------------------------------------------------------------

Security::ReadableView Security::SecureBuffer::GetData() const
{
    return m_buffer;
}

//----------------------------------------------------------------------------------------------------------------------

Security::WriteableView Security::SecureBuffer::GetData()
{
    return m_buffer;
}

//----------------------------------------------------------------------------------------------------------------------

Security::ReadableView Security::SecureBuffer::GetCordon(std::size_t offset, std::size_t size) const
{
    assert(offset + size <= m_buffer.size());
    auto const begin = m_buffer.data() + offset;
    auto const end = begin + size;
    return ReadableView(begin, end);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::SecureBuffer::GetSize() const
{
    return m_buffer.size();
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::SecureBuffer::IsEmpty() const
{
    return m_buffer.empty();
}

//----------------------------------------------------------------------------------------------------------------------

void Security::SecureBuffer::Resize(std::size_t size)
{
    m_buffer.resize(size);
}

//----------------------------------------------------------------------------------------------------------------------

void Security::SecureBuffer::Erase()
{
    EraseMemory(m_buffer.data(), m_buffer.size());
    m_buffer.clear();
}

//----------------------------------------------------------------------------------------------------------------------
