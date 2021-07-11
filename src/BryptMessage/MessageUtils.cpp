//----------------------------------------------------------------------------------------------------------------------
// File: MessageUtils.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "MessageUtils.hpp"
#include "MessageHeader.hpp"
#include "PackUtils.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
//----------------------------------------------------------------------------------------------------------------------

Message::Protocol Message::ConvertToProtocol(std::underlying_type_t<Message::Protocol> protocol)
{
    using ProtocolType = std::underlying_type_t<Message::Protocol>;
    switch (protocol) {
        case static_cast<ProtocolType>(Message::Protocol::Application): {
            return Message::Protocol::Application;
        }
        case static_cast<ProtocolType>(Message::Protocol::Network): {
            return Message::Protocol::Network;
        }
        default: break;
    }
    return Message::Protocol::Invalid;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Message::Protocol> Message::PeekProtocol(std::span<std::uint8_t const> buffer)
{
    constexpr auto ExpectedPosition = sizeof(Message::Protocol);
    if (buffer.size() < ExpectedPosition) { return {}; }

    // The protocol type should be the first byte in the provided message buffer. 
    Message::Protocol protocol = ConvertToProtocol(*buffer.begin());
    if (protocol == Message::Protocol::Invalid) { return {}; }

    return protocol;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<std::uint32_t> Message::PeekSize(std::span<std::uint8_t const> buffer)
{
    // The messagesize section begins after protocol type and version of the message.
    constexpr auto ExpectedPosition = sizeof(Message::Protocol) +
        sizeof(Message::Version::first) +
        sizeof(Message::Version::second);

    // The provided buffer must be large enough to contain the identifier's size
    if (buffer.size() < ExpectedPosition) { return {}; }

    auto begin = buffer.begin() + ExpectedPosition;
    auto end = buffer.end();

    // Attempt to unpack the size from the buffer. 
    std::uint32_t size = 0;
    if (!PackUtils::UnpackChunk(begin, end, size) || size == 0) {
        return {};
    }

    return size;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> Message::PeekSource(
    std::span<std::uint8_t const> buffer)
{
    // The source identifier section begins after protocol type, version, and size of the message.
    // We set the expected position after the size of the identifier as we can return early if 
    // the size byte and first byte are not present. 
    constexpr auto ExpectedPosition = sizeof(Message::Protocol) + 
        sizeof(Message::Version::first) + 
        sizeof(Message::Version::second) +
        sizeof(std::uint32_t) + 
        sizeof(std::uint8_t);

    // The provided buffer must be large enough to contain the identifier's size
    if (buffer.size() < ExpectedPosition) { return {}; }
    
    // Capture the provided identifier size.
    std::uint8_t const size = *(buffer.data() + ExpectedPosition - 1);
        
    // The provided size must not be smaller the smallest possible brypt identifier
    if (size < Node::Identifier::MinimumSize) { return {}; }
    // The provided size must not be larger than the maximum possible brypt identifier
    if (size > Node::Identifier::MaximumSize) { return {}; }

    // The provided buffer must be large enough to contain the data specified by the identifier size
    if (buffer.size() < ExpectedPosition + size) { return {}; }

    auto const start = buffer.begin() + ExpectedPosition;
    auto const stop = buffer.begin() + ExpectedPosition + size;
    auto const identifier = Node::Identifier({ start, stop }, Node::BufferContentType::Network);
    
    if (!identifier.IsValid()) { return {}; }

    return identifier;
}

//----------------------------------------------------------------------------------------------------------------------
