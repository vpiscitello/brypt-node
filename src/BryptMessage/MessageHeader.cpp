//------------------------------------------------------------------------------------------------
// File: MessageHeader.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageHeader.hpp"
#include "PackUtils.hpp"
#include "../BryptIdentifier/IdentifierDefinitions.hpp"
#include "../BryptIdentifier/ReservedIdentifiers.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

Message::Protocol UnpackProtocol(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end);

std::optional<BryptIdentifier::CContainer> UnpackIdentifier(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end);

Message::Destination UnpackDestination(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

CMessageHeader::CMessageHeader()
    : m_protocol(Message::Protocol::Invalid)
    , m_version()
    , m_source()
    , m_destination()
{
}

//------------------------------------------------------------------------------------------------

Message::Protocol CMessageHeader::GetMessageProtocol() const
{
    return m_protocol;
}

//------------------------------------------------------------------------------------------------

Message::Version const& CMessageHeader::GetVersion() const
{
    return m_version;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const& CMessageHeader::GetSourceIdentifier() const
{
    return m_source;
}

//------------------------------------------------------------------------------------------------

Message::Destination CMessageHeader::GetDestinationType() const
{
    return m_destination;
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::CContainer> const& CMessageHeader::GetDestinationIdentifier() const
{
    return m_optDestinationIdentifier;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessageHeader::GetPackSize() const
{
    std::uint32_t size = FixedPackSize();
    size += m_source.NetworkRepresentationSize();
    if (m_optDestinationIdentifier) {
        size += m_optDestinationIdentifier->NetworkRepresentationSize();
    }
    return size;
}

//------------------------------------------------------------------------------------------------

Message::Buffer CMessageHeader::GetPackedBuffer() const
{
	Message::Buffer buffer;
	buffer.reserve(GetPackSize());

    // Header Byte Pack Schema: 
    //  - Section 1 (1 byte): Message Protocol Type
    //  - Section 2 (2 bytes): Message Version (Major, Minor)
    //  - Section 3 (1 byte): Source Identifier Size
    //  - Section 4 (N bytes): Source Identifier
    //  - Section 5 (1 byte): Destination Type
    //      - (Optional) Section 6.1 (1 byte): Destination Identifier Size
    //      - (Optional) Section 6.2 (N bytes): Destination Identifier
    //  - Section 7 (1 byte): Extenstions Count
    //      - Section 7.1 (1 byte): Extension Type      |   Start Repetition
    //      - Section 7.2 (2 bytes): Extension Size     |
    //      - Section 7.3 (N bytes): Extension Data     |   End Repetition

    PackUtils::PackChunk(buffer, m_protocol);
    PackUtils::PackChunk(buffer, m_version.first);
    PackUtils::PackChunk(buffer, m_version.second);
	PackUtils::PackChunk(
		buffer, m_source.GetNetworkRepresentation(), sizeof(std::uint8_t));
    PackUtils::PackChunk(buffer, m_destination);
    if (m_optDestinationIdentifier) {
        PackUtils::PackChunk(
            buffer, m_optDestinationIdentifier->GetNetworkRepresentation(), sizeof(std::uint8_t));
    }
    
    // Extension Packing: Currently, there are no supported extensions of the header. 
    PackUtils::PackChunk(buffer, std::uint8_t(0));

    return buffer;
}

//------------------------------------------------------------------------------------------------

bool CMessageHeader::IsValid() const
{	
	// A header must identify a valid message protocol
	if (m_protocol == static_cast<Message::Protocol>(Message::Protocol::Invalid)) {
		return false;
	}

	// A header must have a valid brypt source identifier attached
	if (!m_source.IsValid()) {
		return false;
	}

	return true;
}

//------------------------------------------------------------------------------------------------

constexpr std::uint32_t CMessageHeader::FixedPackSize() const
{
	std::uint32_t size = 0;
	size += sizeof(m_protocol); // 1 byte for message protocol type 
	size += sizeof(m_version.first); // 1 byte for major version
	size += sizeof(m_version.second); // 1 byte for minor version
    size += sizeof(std::uint8_t); // 1 byte for source identifier size
    size += sizeof(std::uint8_t); // 1 byte for destination type
    size += sizeof(std::uint8_t); // 1 byte for destination identifier size
    size += sizeof(std::uint8_t); // 1 bytes for header extension size
	return size;
}

//------------------------------------------------------------------------------------------------

bool CMessageHeader::ParseBuffer(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end)
{
    // The buffer must contain at least the minimum bytes packed by a header.
    if (auto const size = std::distance(begin, end); size < FixedPackSize()) {
        return false;
    }

    // Unpack the message protocol
    m_protocol = local::UnpackProtocol(begin, end);
    // If the unpacked message protocol is invalid there is no need to contianue
    if (m_protocol == Message::Protocol::Invalid) {
        return false;
    }

    // Unpack the message major and minor version numbers
    if (!PackUtils::UnpackChunk(begin, end, m_version.first)) {
        return false;
    }
    if (!PackUtils::UnpackChunk(begin, end, m_version.second)) {
        return false;
    }
    
    // Unpack the source identifier
    auto const optSource = local::UnpackIdentifier(begin, end);
    if (!optSource) {
        return false;
    }
    m_source = *optSource;

    if (m_destination = local::UnpackDestination(begin, end);
        m_destination == Message::Destination::Invalid) {
        return false;
    }

    if (m_destination == Message::Destination::Node) {
        m_optDestinationIdentifier = local::UnpackIdentifier(begin, end);
    }

    // Unpack the number of extensions. 
    std::uint8_t extensions = 0;
    if (!PackUtils::UnpackChunk(begin, end, extensions)) {
        return false;
    }

    return true;
}

//------------------------------------------------------------------------------------------------

Message::Protocol local::UnpackProtocol(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end)
{
    using ProtocolType = std::underlying_type_t<Message::Protocol>;

    ProtocolType protocol = 0;
    PackUtils::UnpackChunk(begin, end, protocol);

    switch (protocol) {
        case static_cast<ProtocolType>(Message::Protocol::Application): {
            return Message::Protocol::Application;
        }
        case static_cast<ProtocolType>(Message::Protocol::Handshake): {
            return Message::Protocol::Handshake;
        }
        default: return Message::Protocol::Invalid;
    }
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::CContainer> local::UnpackIdentifier(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end)
{
    std::uint8_t size = 0; 
    if (!PackUtils::UnpackChunk(begin, end, size)) {
        return {};
    }

    if (size < BryptIdentifier::Network::MinimumLength || size > BryptIdentifier::Network::MaximumLength) {
        return {};
    }

    BryptIdentifier::BufferType buffer;
    if (!PackUtils::UnpackChunk(begin, end, buffer, size)) {
        return {};
    }

    return BryptIdentifier::CContainer(
        buffer, BryptIdentifier::BufferContentType::Network);
}

//------------------------------------------------------------------------------------------------

Message::Destination local::UnpackDestination(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end)
{
    using DestinationType = std::underlying_type_t<Message::Destination>;

    DestinationType destination = 0;
    PackUtils::UnpackChunk(begin, end, destination);

    switch (destination) {
        case static_cast<DestinationType>(Message::Destination::Cluster): {
            return Message::Destination::Cluster;
        }
        case static_cast<DestinationType>(Message::Destination::Network): {
            return Message::Destination::Network;
        }
        case static_cast<DestinationType>(Message::Destination::Node): {
            return Message::Destination::Node;
        }
        default: return Message::Destination::Invalid;
    }
}

//------------------------------------------------------------------------------------------------