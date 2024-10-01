//----------------------------------------------------------------------------------------------------------------------
// File: MessageHeader.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
#include "PackUtils.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
#include "Utilities/TimeUtils.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <span>
#include <utility>
//----------------------------------------------------------------------------------------------------------------------

namespace Message::Application { class Builder; }
namespace Message::Platform { class Builder; }

//----------------------------------------------------------------------------------------------------------------------
namespace Message {
//----------------------------------------------------------------------------------------------------------------------

class Header;

//----------------------------------------------------------------------------------------------------------------------
} // Message namespace
//----------------------------------------------------------------------------------------------------------------------

class Message::Header
{
public:
	friend class Message::Application::Builder;
    friend class Message::Platform::Builder;

	Header();

	[[nodiscard]] bool operator==(Header const& other) const;

	Protocol GetMessageProtocol() const;
    Version const& GetVersion() const;
    std::uint32_t GetMessageSize() const;
    Node::Identifier const& GetSource() const;
    Destination GetDestinationType() const;
    std::optional<Node::Identifier> const& GetDestination() const;
	TimeUtils::Timestamp const& GetTimestamp() const;

    std::size_t GetPackSize() const;
	Buffer GetPackedBuffer() const;

	bool IsValid() const;

	static constexpr std::size_t FixedPackSize()
    {
        std::size_t size = 0;
        size += sizeof(m_protocol); // 1 byte for message protocol type 
        size += sizeof(m_version.first); // 1 byte for major version
        size += sizeof(m_version.second); // 1 byte for minor version
        size += sizeof(m_size); // 4 bytes for message size
        size += sizeof(std::uint8_t); // 1 byte for source identifier size
        size += sizeof(std::uint8_t); // 1 byte for destination type
        size += sizeof(std::uint8_t); // 1 byte for destination identifier size
        size += sizeof(std::uint8_t); // 1 bytes for header extension size
        size += sizeof(std::uint64_t); // 8 bytes for timestamp
        assert(std::in_range<std::uint16_t>(size));
        return size;
    }

    static constexpr std::size_t PeekableEncodedSize()
    {
        std::size_t size = 0;
        size += sizeof(m_protocol); // 1 byte for message protocol type 
        size += sizeof(m_version.first); // 1 byte for major version
        size += sizeof(m_version.second); // 1 byte for minor version
        size += sizeof(m_size); // 4 bytes for message size
        size += sizeof(std::uint8_t); // 1 byte for source identifier size
        auto const encoded = Z85::EncodedSize(size);
        assert(std::in_range<std::uint16_t>(size));
        return encoded;
    }

    static constexpr std::size_t MaximumEncodedSize()
    {
        // Base the peekable encoded size 
        std::size_t size = FixedPackSize();
        // Account for the maximum possible sizes of the source and destination sizes.
        size += Node::Identifier::MaximumSize;
        size += Node::Identifier::MaximumSize;
        auto const encoded = Z85::EncodedSize(size);
        assert(std::in_range<std::uint16_t>(encoded));
        return encoded;
    }

private:
    [[nodiscard]] bool ParseBuffer(
        std::span<std::uint8_t const>::iterator& begin,
        std::span<std::uint8_t const>::iterator const& end);

    Protocol m_protocol;
    Version m_version;
    std::uint32_t m_size;
    Node::Identifier m_source;
    Destination m_destination;
    std::optional<Node::Identifier> m_optDestinationIdentifier;
	TimeUtils::Timestamp m_timestamp; // The message creation timestamp
};

//----------------------------------------------------------------------------------------------------------------------
