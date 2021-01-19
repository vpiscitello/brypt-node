//------------------------------------------------------------------------------------------------
// File: MessageHeader.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageHeader.hpp"
#include "MessageUtils.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

Message::Protocol UnpackProtocol(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end);

std::optional<BryptIdentifier::Container> UnpackIdentifier(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end);

Message::Destination UnpackDestination(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end);

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

MessageHeader::MessageHeader()
    : m_protocol(Message::Protocol::Invalid)
    , m_version()
    , m_size(0)
    , m_source()
    , m_destination(Message::Destination::Node)
    , m_optDestinationIdentifier()
{
}

//------------------------------------------------------------------------------------------------

Message::Protocol MessageHeader::GetMessageProtocol() const
{
    return m_protocol;
}

//------------------------------------------------------------------------------------------------

Message::Version const& MessageHeader::GetVersion() const
{
    return m_version;
}

//------------------------------------------------------------------------------------------------

std::uint32_t MessageHeader::GetMessageSize() const
{
    return m_size;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::Container const& MessageHeader::GetSourceIdentifier() const
{
    return m_source;
}

//------------------------------------------------------------------------------------------------

Message::Destination MessageHeader::GetDestinationType() const
{
    return m_destination;
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::Container> const& MessageHeader::GetDestinationIdentifier() const
{
    return m_optDestinationIdentifier;
}

//------------------------------------------------------------------------------------------------

std::uint32_t MessageHeader::GetPackSize() const
{
    std::size_t size = FixedPackSize();
    size += m_source.NetworkRepresentationSize();
    if (m_optDestinationIdentifier) {
        size += m_optDestinationIdentifier->NetworkRepresentationSize();
    }
    return static_cast<std::uint32_t>(size);
}

//------------------------------------------------------------------------------------------------

Message::Buffer MessageHeader::GetPackedBuffer() const
{
	Message::Buffer buffer;
	buffer.reserve(GetPackSize());

    // Header Byte Pack Schema: 
    //  - Section 1 (1 byte): Message Protocol Type
    //  - Section 2 (2 bytes): Message Version (Major, Minor)
    //  - Section 3 (4 bytes): Message Size
    //  - Section 4 (1 byte): Source Identifier Size
    //  - Section 5 (N bytes): Source Identifier
    //  - Section 6 (1 byte): Destination Type
    //      - (Optional) Section 6.1 (1 byte): Destination Identifier Size
    //      - (Optional) Section 6.2 (N bytes): Destination Identifier
    //  - Section 7 (1 byte): Extenstions Count
    //      - Section 7.1 (1 byte): Extension Type      |   Start Repetition
    //      - Section 7.2 (2 bytes): Extension Size     |
    //      - Section 7.3 (N bytes): Extension Data     |   End Repetition

    PackUtils::PackChunk(m_protocol, buffer);
    PackUtils::PackChunk(m_version.first, buffer);
    PackUtils::PackChunk(m_version.second, buffer);
    PackUtils::PackChunk(m_size, buffer);
	PackUtils::PackChunk<std::uint8_t>(m_source.GetNetworkRepresentation(), buffer);
    PackUtils::PackChunk(m_destination, buffer);

    // If a destination as been set pack the size and the identifier. 
    // Otherwise, indicate there is no destination identifier.
    if (m_optDestinationIdentifier) {
        PackUtils::PackChunk<std::uint8_t>(
            m_optDestinationIdentifier->GetNetworkRepresentation(), buffer);
    } else {
        PackUtils::PackChunk(std::uint8_t(0), buffer);
    }
    
    // Extension Packing: Currently, there are no supported extensions of the header. 
    PackUtils::PackChunk(std::uint8_t(0), buffer);

    return buffer;
}

//------------------------------------------------------------------------------------------------

bool MessageHeader::IsValid() const
{	
	// A header must identify a valid message protocol
	if (m_protocol == Message::Protocol::Invalid) {
		return false;
	}

    // A header must contain the size of the message. This msut be non-zero.
	if (m_size == 0) { return false; }

	// A header must have a valid brypt source identifier attached
	if (!m_source.IsValid()) { return false; }

	return true;
}

//------------------------------------------------------------------------------------------------

bool MessageHeader::ParseBuffer(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end)
{
    // The buffer must contain at least the minimum bytes packed by a header.
    if (std::cmp_less(std::distance(begin, end), FixedPackSize())) {
        return false;
    }

    // Unpack the message protocol
    m_protocol = local::UnpackProtocol(begin, end);
    // If the unpacked message protocol is invalid there is no need to contianue
    if (m_protocol == Message::Protocol::Invalid) { return false; }

    // Unpack the message major and minor version numbers
    if (!PackUtils::UnpackChunk(begin, end, m_version.first)) { return false; }
    if (!PackUtils::UnpackChunk(begin, end, m_version.second)) { return false; }

    // Unpack the message size
    if (!PackUtils::UnpackChunk(begin, end, m_size)) { return false; }

    // Unpack the source identifier
    auto const optSource = local::UnpackIdentifier(begin, end);
    if (!optSource) {  return false; }
    m_source = *optSource;

    if (m_destination = local::UnpackDestination(begin, end);
        m_destination == Message::Destination::Invalid) {
        return false;
    }

    m_optDestinationIdentifier = local::UnpackIdentifier(begin, end);

    // Unpack the number of extensions. 
    std::uint8_t extensions = 0;
    if (!PackUtils::UnpackChunk(begin, end, extensions)) { return false; }

    return true;
}

//------------------------------------------------------------------------------------------------

Message::Protocol local::UnpackProtocol(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end)
{
    using ProtocolType = std::underlying_type_t<Message::Protocol>;

    ProtocolType protocol = 0;
    PackUtils::UnpackChunk(begin, end, protocol);
    return Message::ConvertToProtocol(protocol);
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::Container> local::UnpackIdentifier(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end)
{
    std::uint8_t size = 0; 
    if (!PackUtils::UnpackChunk(begin, end, size)) { return {}; }

    if (size < BryptIdentifier::Network::MinimumLength || size > BryptIdentifier::Network::MaximumLength) {
        return {};
    }

    std::vector<std::uint8_t> buffer;
    buffer.reserve(size);
    if (!PackUtils::UnpackChunk(begin, end, buffer)) { return {}; }

    return BryptIdentifier::Container(
        buffer, BryptIdentifier::BufferContentType::Network);
}

//------------------------------------------------------------------------------------------------

Message::Destination local::UnpackDestination(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end)
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
