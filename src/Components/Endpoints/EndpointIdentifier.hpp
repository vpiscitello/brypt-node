//------------------------------------------------------------------------------------------------
// File: EndpointIdentifier.hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <limits>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Endpoints {
//------------------------------------------------------------------------------------------------

using EndpointIdType = std::int32_t;

constexpr EndpointIdType InvalidEndpointIdentifier = std::numeric_limits<EndpointIdType>::min();

//------------------------------------------------------------------------------------------------
} // Endpoints namespace
//------------------------------------------------------------------------------------------------

class CEndpointIdentifierGenerator
{
public:
    static CEndpointIdentifierGenerator& Instance() {
        static CEndpointIdentifierGenerator instance;
        return instance;
    }

    CEndpointIdentifierGenerator(CEndpointIdentifierGenerator const&) = delete;
    void operator=(CEndpointIdentifierGenerator const&) = delete;

    Endpoints::EndpointIdType GetIdentifier() { return ++m_identifier; }
    
private:
    CEndpointIdentifierGenerator()
        : m_identifier(0)
    {
    }

    Endpoints::EndpointIdType m_identifier;
};

//------------------------------------------------------------------------------------------------