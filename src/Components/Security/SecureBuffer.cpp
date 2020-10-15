//------------------------------------------------------------------------------------------------
// File: SecureBuffer.cpp
// Description: 
//------------------------------------------------------------------------------------------------
#include "SecureBuffer.hpp"
#include "SecurityUtils.hpp"
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

std::uint8_t const* Security::CSecureBuffer::GetData() const
{
    return m_buffer.data();
}

//------------------------------------------------------------------------------------------------

std::uint32_t Security::CSecureBuffer::GetSize() const
{
    return m_buffer.size();
}

//------------------------------------------------------------------------------------------------
