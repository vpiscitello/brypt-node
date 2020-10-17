//------------------------------------------------------------------------------------------------
// File: SecureBuffer.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "SecurityTypes.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Security {
//------------------------------------------------------------------------------------------------

class CSecureBuffer;

//------------------------------------------------------------------------------------------------
} // Security namespace
//------------------------------------------------------------------------------------------------

class Security::CSecureBuffer
{
public:
    CSecureBuffer(Security::Buffer&& buffer);
    ~CSecureBuffer();

    std::uint8_t const* GetData() const;
    std::uint32_t GetSize() const;
    
private:
    Security::Buffer m_buffer;

};

//------------------------------------------------------------------------------------------------