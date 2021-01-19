//------------------------------------------------------------------------------------------------
// File: MessageSink.hpp
// Description: Defines an interface that allows endpoints to forward and receive 
// messages from the central message queue.
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "BryptMessage/MessageTypes.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>
//------------------------------------------------------------------------------------------------

class CBryptPeer;
class CMessageContext;

//------------------------------------------------------------------------------------------------

class IMessageSink
{
public:
    virtual ~IMessageSink() = default;

    [[nodiscard]] virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        std::string_view buffer) = 0;
        
    [[nodiscard]] virtual bool CollectMessage(
        std::weak_ptr<CBryptPeer> const& wpBryptPeer,
        CMessageContext const& context,
        Message::Buffer const& buffer) = 0;
};

//------------------------------------------------------------------------------------------------