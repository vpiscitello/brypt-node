//----------------------------------------------------------------------------------------------------------------------
// File: NetworkMessage.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "NetworkMessage.hpp"
#include "PackUtils.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

Message::Network::Type UnpackMessageType(
	std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end);

//----------------------------------------------------------------------------------------------------------------------
namespace Extensions {
//----------------------------------------------------------------------------------------------------------------------

enum Types : std::uint8_t { Invalid = 0x00 };

//----------------------------------------------------------------------------------------------------------------------
} // Extensions namespace 
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Message::Network::Parcel::Parcel()
	: m_context()
	, m_header()
	, m_type(Message::Network::Type::Invalid)
	, m_payload()
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Parcel::Parcel(Parcel const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_type(other.m_type)
	, m_payload(other.m_payload)
{
}
//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder Message::Network::Parcel::GetBuilder() { return Builder{}; }

//----------------------------------------------------------------------------------------------------------------------

Message::Context const& Message::Network::Parcel::GetContext() const { return m_context; }

//----------------------------------------------------------------------------------------------------------------------

Message::Header const& Message::Network::Parcel::GetHeader() const { return m_header; }

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& Message::Network::Parcel::GetSource() const { return m_header.GetSource(); }

//----------------------------------------------------------------------------------------------------------------------

Message::Destination Message::Network::Parcel::GetDestinationType() const { return m_header.GetDestinationType(); }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> const& Message::Network::Parcel::GetDestination() const
{
	return m_header.GetDestination();
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Type Message::Network::Parcel::GetType() const { return m_type; }

//----------------------------------------------------------------------------------------------------------------------

Message::Buffer const& Message::Network::Parcel::GetPayload() const { return m_payload; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Network::Parcel::GetPackSize() const
{
	std::size_t size = FixedPackSize();
	size += m_header.GetPackSize();
	size += m_payload.size();

	auto const encoded = Z85::EncodedSize(size);
    assert(std::in_range<std::uint32_t>(encoded));
	return encoded;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Pack the Message class values into a single raw string.
//----------------------------------------------------------------------------------------------------------------------
std::string Message::Network::Parcel::GetPack() const
{
	Message::Buffer buffer = m_header.GetPackedBuffer();
	buffer.reserve(m_header.GetMessageSize());
    // Network Pack Schema: 
    //  - Section 1 (1 bytes): Network Message Type
    //  - Section 2 (4 bytes): Network Payload Size
    //  - Section 3 (N bytes): Network Payload
    //  - Section 4 (1 byte): Extenstions Count
    //      - Section 4.1 (1 byte): Extension Type      |   Extension Start
    //      - Section 4.2 (2 bytes): Extension Size     |
    //      - Section 4.3 (N bytes): Extension Data     |   Extension End

	PackUtils::PackChunk(m_type, buffer);
	PackUtils::PackChunk<std::uint32_t>(m_payload, buffer);

	// Extension Packing
	PackUtils::PackChunk(std::uint8_t(0), buffer);

	// Calculate the number of bytes needed to pad to next 4 byte boundary
	auto const paddingBytesRequired = (4 - (buffer.size() & 3)) & 3;
	// Pad the message to ensure the encoding method doesn't add padding to the end of the message.
	Message::Buffer padding(paddingBytesRequired, 0);
	buffer.insert(buffer.end(), padding.begin(), padding.end());

	return Z85::Encode(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

Message::ShareablePack Message::Network::Parcel::GetShareablePack() const
{
	return std::make_shared<std::string const>(GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

Message::ValidationStatus Message::Network::Parcel::Validate() const
{	
	// A message must have a valid header
	if (!m_header.IsValid()) {
		return Message::ValidationStatus::Error;
	}

	// The network message type must not be invalid.
	if (m_type == Message::Network::Type::Invalid) { return Message::ValidationStatus::Error; }

	return Message::ValidationStatus::Success;
}

//----------------------------------------------------------------------------------------------------------------------

constexpr std::size_t Message::Network::Parcel::FixedPackSize() const
{
	std::size_t size = 0;
	size += sizeof(Message::Network::Type); // 1 byte for network message type
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += sizeof(std::uint8_t); // 1 byte for extensions size
    assert(std::in_range<std::uint32_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder::Builder()
    : m_parcel()
	, m_hasStageFailure(false)
{
	m_parcel.m_header.m_protocol = Message::Protocol::Network;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& Message::Network::Builder::GetSource() const
{
	return m_parcel.m_header.m_source;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> const& Message::Network::Builder::GetDestination() const
{
	return m_parcel.m_header.m_optDestinationIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetContext(Context const& context)
{
	m_parcel.m_context = context;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetSource(Node::Identifier const& identifier)
{
	m_parcel.m_header.m_source = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetSource(
    Node::Internal::Identifier const& identifier)
{
	m_parcel.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetSource(std::string_view identifier)
{
	m_parcel.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetDestination(Node::Identifier const& identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetDestination(
    Node::Internal::Identifier const& identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetDestination(std::string_view identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::MakeHandshakeMessage()
{
	m_parcel.m_type = Message::Network::Type::Handshake;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::MakeHeartbeatRequest()
{
	m_parcel.m_type = Message::Network::Type::HeartbeatRequest;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::MakeHeartbeatResponse()
{
	m_parcel.m_type = Message::Network::Type::HeartbeatResponse;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetPayload(std::string_view buffer)
{
    return SetPayload({ reinterpret_cast<std::uint8_t const*>(buffer.data()), buffer.size() });
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::SetPayload(std::span<std::uint8_t const> buffer)
{
    m_parcel.m_payload = Message::Buffer(buffer.begin(), buffer.end());
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::FromDecodedPack(std::span<std::uint8_t const> buffer)
{
    if (!buffer.empty()) { Unpack(buffer); }
	else { m_hasStageFailure = true; }
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder& Message::Network::Builder::FromEncodedPack(std::string_view pack)
{
    if (!pack.empty()) { Unpack(Z85::Decode(pack)); }
	else { m_hasStageFailure = true; }
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Parcel&& Message::Network::Builder::Build()
{
	m_parcel.m_header.m_size = static_cast<std::uint32_t>(m_parcel.GetPackSize());

    return std::move(m_parcel);
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Builder::OptionalParcel Message::Network::Builder::ValidatedBuild()
{
	m_parcel.m_header.m_size = static_cast<std::uint32_t>(m_parcel.GetPackSize());

    if (m_hasStageFailure || m_parcel.Validate() != Message::ValidationStatus::Success) { return {}; }
    return std::move(m_parcel);
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Unpack the raw message string into the Message class variables.
//----------------------------------------------------------------------------------------------------------------------
void Message::Network::Builder::Unpack(std::span<std::uint8_t const> buffer)
{
	auto begin = buffer.begin();
	auto end = buffer.end();

	if (!m_parcel.m_header.ParseBuffer(begin, end)) { return; }

	m_parcel.m_type = local::UnpackMessageType(begin, end);
	if (m_parcel.m_type == Message::Network::Type::Invalid) { return; }

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_parcel.m_header.m_protocol != Message::Protocol::Network) { return; }

	{
		std::uint32_t size = 0;
		if (!PackUtils::UnpackChunk(begin, end, size)) { return; }
		if (!PackUtils::UnpackChunk(begin, end, m_parcel.m_payload, size)) { return; }
	}

	std::uint8_t extensions = 0;
	if (!PackUtils::UnpackChunk(begin, end, extensions)) { return; }

	if (extensions != 0) { UnpackExtensions(begin, end); }
}

//----------------------------------------------------------------------------------------------------------------------

void Message::Network::Builder::UnpackExtensions(
	std::span<std::uint8_t const>::iterator& begin,
	std::span<std::uint8_t const>::iterator const& end)
{
	while (begin != end) {
		using ExtensionType = std::underlying_type_t<local::Extensions::Types>;
		ExtensionType extension = 0;
		PackUtils::UnpackChunk(begin, end, extension);

		switch (extension) {				
			default: return;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Type local::UnpackMessageType(
	std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end)
{
	using namespace Message::Network;

	using NetworkType = std::underlying_type_t<Type>;
	NetworkType type = 0;
    PackUtils::UnpackChunk(begin, end, type);

    switch (type) {
        case static_cast<NetworkType>(Type::Handshake):
        case static_cast<NetworkType>(Type::HeartbeatRequest):
        case static_cast<NetworkType>(Type::HeartbeatResponse): {
            return static_cast<Type>(type);
        }
        default: return Type::Invalid;
    }
}

//----------------------------------------------------------------------------------------------------------------------
