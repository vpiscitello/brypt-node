//------------------------------------------------------------------------------------------------
// File: MessageHeader.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
#include "PackUtils.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "BryptIdentifier/IdentifierDefinitions.hpp"
#include "Utilities/Z85.hpp"
//------------------------------------------------------------------------------------------------
#include <cstdint>
#include <utility>
//------------------------------------------------------------------------------------------------

class CMessageHeader {
public:
	CMessageHeader();

	// CApplicationBuilder {
	friend class CApplicationBuilder;
    friend class CNetworkBuilder;
    // } CApplicationBuilder 

	Message::Protocol GetMessageProtocol() const;
    Message::Version const& GetVersion() const;
    std::uint32_t GetMessageSize() const;
    BryptIdentifier::CContainer const& GetSourceIdentifier() const;
    Message::Destination GetDestinationType() const;
    std::optional<BryptIdentifier::CContainer> const& GetDestinationIdentifier() const;

    std::uint32_t GetPackSize() const;
	Message::Buffer GetPackedBuffer() const;

	bool IsValid() const;

	constexpr static std::uint32_t FixedPackSize()
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
        return static_cast<std::uint32_t>(size);
    }

    constexpr static std::uint32_t PeekableEncodedSize()
    {
        std::size_t size = 0;
        size += sizeof(m_protocol); // 1 byte for message protocol type 
        size += sizeof(m_version.first); // 1 byte for major version
        size += sizeof(m_version.second); // 1 byte for minor version
        size += sizeof(m_size); // 4 bytes for message size
        size += sizeof(std::uint8_t); // 1 byte for source identifier size
        return static_cast<std::uint32_t>(Z85::EncodedSize(size));
    }

    constexpr static std::uint32_t MaximumEncodedSize()
    {
        // Base the peekable encoded size 
        std::size_t size = FixedPackSize();
        // Account for the maximum possible sizes of the source and destination sizes.
        size += BryptIdentifier::Network::MaximumLength;
        size += BryptIdentifier::Network::MaximumLength;
        return static_cast<std::uint32_t>(Z85::EncodedSize(size));
    }

private:
    bool ParseBuffer(
        Message::Buffer::const_iterator& begin,
        Message::Buffer::const_iterator const& end);

    Message::Protocol m_protocol;
    Message::Version m_version;
    std::uint32_t m_size;
    BryptIdentifier::CContainer m_source;
    Message::Destination m_destination;

    std::optional<BryptIdentifier::CContainer> m_optDestinationIdentifier;

};

//------------------------------------------------------------------------------------------------
