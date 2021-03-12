//----------------------------------------------------------------------------------------------------------------------
// File: MessageSink.hpp
// Description: Defines an interface that allows endpoints to forward and receive 
// messages from the central message queue.
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "BryptMessage/MessageTypes.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

class BryptPeer;
class MessageContext;

//----------------------------------------------------------------------------------------------------------------------

class IMessageSink
{
public:
    virtual ~IMessageSink() = default;

    [[nodiscard]] virtual bool CollectMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        MessageContext const& context,
        std::string_view buffer) = 0;
        
    [[nodiscard]] virtual bool CollectMessage(
        std::weak_ptr<BryptPeer> const& wpBryptPeer,
        MessageContext const& context,
        std::span<std::uint8_t const> buffer) = 0;
};

//----------------------------------------------------------------------------------------------------------------------