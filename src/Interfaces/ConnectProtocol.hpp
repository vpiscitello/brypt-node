//------------------------------------------------------------------------------------------------
// File: .hpp
// Description: 
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <memory>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CMessageContext;

//------------------------------------------------------------------------------------------------

class IConnectProtocol
{
public:
    virtual ~IConnectProtocol() = default;

    virtual bool SendRequest(
        BryptIdentifier::SharedContainer const& spSourceIdentifier,
        std::shared_ptr<CBryptPeer> const& spBryptPeer,
        CMessageContext const& context) const = 0;
};

//------------------------------------------------------------------------------------------------
