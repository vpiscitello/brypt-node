//------------------------------------------------------------------------------------------------
// File: MessageUtils.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageUtils.hpp"
#include "MessageHeader.hpp"
#include "PackUtils.hpp"
#include "../BryptIdentifier/BryptIdentifier.hpp"
#include "../BryptIdentifier/IdentifierDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <algorithm>
//------------------------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------------------------

std::optional<Message::Protocol> Message::PeekProtocol(
    Message::Buffer::const_iterator const& begin,
    Message::Buffer::const_iterator const& end)
{
    if (begin == end) { return {}; }

    if (auto const size = std::distance(begin, end); size < 1) {
        return {};
    }

    // The protocol type should be the first byte in the provided message buffer. 
    Message::Protocol protocol = ConvertToProtocol(*begin);
    if (protocol == Message::Protocol::Invalid) { return {}; }

    return protocol;
}

//------------------------------------------------------------------------------------------------

std::optional<std::uint32_t> Message::PeekSize(
    Message::Buffer::const_iterator const& begin,
    Message::Buffer::const_iterator const& end)
{
    if (begin == end) { return {}; }

    // The messagesize section begins after protocol type and version of the message.
    auto start = begin +
        sizeof(Message::Protocol) + 
        sizeof(Message::Version::first) + 
        sizeof(Message::Version::second);

    std::uint32_t size = 0;
    
    // The provided buffer must be large enough to contain the identifier's size
    if (auto const bytes = std::distance(start, end); bytes < 1) { return {}; }

    if (!PackUtils::UnpackChunk(start, end, size) || size == 0) { return {}; }

    return size;
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::CContainer> Message::PeekSource(
    Message::Buffer::const_iterator const& begin,
    Message::Buffer::const_iterator const& end)
{
    if (begin == end) { return {}; }

    // The source identifier section begins after protocol type, version, and size of the message.
    auto start = begin +
        sizeof(Message::Protocol) + 
        sizeof(Message::Version::first) + 
        sizeof(Message::Version::second) +
        sizeof(std::uint32_t);

    // The provided buffer must be large enough to contain the identifier's size
    if (auto const size = std::distance(start, end); size < 1) { return {}; }
    
    // Capture the provided identifier size.
    std::uint8_t const size = *start;
        
    // The provided size must not be smaller the smallest possible brypt identifier
    if (size < BryptIdentifier::Network::MinimumLength) { return {}; }
    // The provided size must not be larger than the maximum possible brypt identifier
    if (size > BryptIdentifier::Network::MaximumLength) { return {}; }

    ++start;  // Move to the start of the source identifier field. 
    
    // The provided buffer must be large enough to contain the data specified by the identifier size
    if (auto const bytes = std::distance(start, end); bytes < size) {
        return {};
    }

    auto const stop = start + size;
    auto const identifier = BryptIdentifier::CContainer(
        { start, stop }, BryptIdentifier::BufferContentType::Network);
    
    if (!identifier.IsValid()) { return {}; }

    return identifier;
}

//------------------------------------------------------------------------------------------------
