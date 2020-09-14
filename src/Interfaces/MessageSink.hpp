//------------------------------------------------------------------------------------------------
// File: MessageSink.hpp
// Description: Defines an interface that allows endpoints to forward and receive 
// messages from the central message queue.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include <functional>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CMessage;

//------------------------------------------------------------------------------------------------

class IMessageSink
{
public:
    virtual ~IMessageSink() = default;

    virtual void CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessage const& message) = 0;
};

//------------------------------------------------------------------------------------------------