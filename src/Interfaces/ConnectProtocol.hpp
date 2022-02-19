//----------------------------------------------------------------------------------------------------------------------
// File: ConnectProtocol.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptIdentifier/IdentifierTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

class MessageContext;

//----------------------------------------------------------------------------------------------------------------------

class IConnectProtocol
{
public:
    virtual ~IConnectProtocol() = default;

    virtual bool SendRequest(
        Node::SharedIdentifier const& spSourceIdentifier,
        std::shared_ptr<Peer::Proxy> const& spPeerProxy,
        Message::Context const& context) const = 0;
};

//----------------------------------------------------------------------------------------------------------------------
