//------------------------------------------------------------------------------------------------
// File: MessageUtils.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageUtils.hpp"
#include "MessageHeader.hpp"
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
        case static_cast<ProtocolType>(Message::Protocol::Handshake): {
            return Message::Protocol::Handshake;
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
    if (begin == end) {
        return {};
    }

    if (auto const size = std::distance(begin, end); size < 1) {
        return {};
    }

    // The protocol type should be the first byte in the provided message buffer. 
    Message::Protocol protocol = ConvertToProtocol(*begin);
    if (protocol == Message::Protocol::Invalid) {
        return {};
    }

    return protocol;
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::CContainer> Message::PeekSource(
    Message::Buffer::const_iterator const& begin,
    Message::Buffer::const_iterator const& end)
{
    if (begin == end) {
        return {};
    }

    // The source identifier section begins after protocol type and version of the message.
    auto const start = sizeof(Message::Protocol) + 
        sizeof(Message::Version::first) + 
        sizeof(Message::Version::second);

    auto identifierStart = begin + start;

    // The provided buffer must be large enough to contain the identifier's size
    if (std::uint32_t const size = std::distance(identifierStart, end); size < 1) {
        return {};
    }
    
    // Capture the provided identifier size.
    std::uint8_t const identifierSize = *identifierStart;
        
    // The provided size must not be smaller the smallest possible brypt identifier
    if (identifierSize < BryptIdentifier::Network::MinimumLength) {
        return {};
    }
    // The provided size must not be larger than the maximum possible brypt identifier
    if (identifierSize > BryptIdentifier::Network::MaximumLength) {
        return {};
    }

    ++identifierStart;  // Move to the start of the source identifier field. 
    
    // The provided buffer must be large enough to contain the data specified by the identifier size
    if (std::uint32_t const size = std::distance(identifierStart, end); size < identifierSize) {
        return {};
    }

    auto const identifierEnd = identifierStart + identifierSize;

    auto const identifier = BryptIdentifier::CContainer(
        { identifierStart, identifierEnd }, BryptIdentifier::BufferContentType::Network);
    
    if (!identifier.IsValid()) {
        return {};
    }

    return identifier;
}

//------------------------------------------------------------------------------------------------
