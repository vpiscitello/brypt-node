//----------------------------------------------------------------------------------------------------------------------
// File: EndpointIdentifier.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <limits>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Network::Endpoint {
//----------------------------------------------------------------------------------------------------------------------

using Identifier = std::int32_t;

constexpr Identifier InvalidIdentifier = std::numeric_limits<Identifier>::min();

class IdentifierGenerator;

//----------------------------------------------------------------------------------------------------------------------
} // Network::Network namespace
//----------------------------------------------------------------------------------------------------------------------

class Network::Endpoint::IdentifierGenerator
{
public:
    static IdentifierGenerator& Instance() {
        static IdentifierGenerator instance;
        return instance;
    }

    IdentifierGenerator(IdentifierGenerator const&) = delete;
    void operator=(IdentifierGenerator const&) = delete;

    Endpoint::Identifier Generate() { return ++m_identifier; }
    
private:
    IdentifierGenerator()
        : m_identifier(0)
    {
    }

    Identifier m_identifier;
};

//----------------------------------------------------------------------------------------------------------------------