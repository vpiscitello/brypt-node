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

class BryptPeer;
class MessageContext;

//------------------------------------------------------------------------------------------------

class IConnectProtocol
{
public:
    virtual ~IConnectProtocol() = default;

    virtual bool SendRequest(
        BryptIdentifier::SharedContainer const& spSourceIdentifier,
        std::shared_ptr<BryptPeer> const& spBryptPeer,
        MessageContext const& context) const = 0;
};

//------------------------------------------------------------------------------------------------
