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

namespace Message { class Context; }

//----------------------------------------------------------------------------------------------------------------------

class IMessageSink
{
public:
    virtual ~IMessageSink() = default;
    [[nodiscard]] virtual bool CollectMessage(Message::Context const& context, std::string_view buffer) = 0;
    [[nodiscard]] virtual bool CollectMessage(Message::Context const& context, std::span<std::uint8_t const> buffer) = 0;
};

//----------------------------------------------------------------------------------------------------------------------