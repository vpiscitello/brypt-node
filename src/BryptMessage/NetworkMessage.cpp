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

NetworkMessage::NetworkMessage()
	: m_context()
	, m_header()
	, m_type(Message::Network::Type::Invalid)
	, m_payload()
{
}

//----------------------------------------------------------------------------------------------------------------------

NetworkMessage::NetworkMessage(NetworkMessage const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_type(other.m_type)
	, m_payload(other.m_payload)
{
}
//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder NetworkMessage::Builder()
{
	return NetworkBuilder{};
}

//----------------------------------------------------------------------------------------------------------------------

MessageContext const& NetworkMessage::GetContext() const
{
	return m_context;
}

//----------------------------------------------------------------------------------------------------------------------

MessageHeader const& NetworkMessage::GetMessageHeader() const
{
	return m_header;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& NetworkMessage::GetSourceIdentifier() const
{
	return m_header.GetSourceIdentifier();
}

//----------------------------------------------------------------------------------------------------------------------

Message::Destination NetworkMessage::GetDestinationType() const
{
	return m_header.GetDestinationType();
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> const& NetworkMessage::GetDestinationIdentifier() const
{
	return m_header.GetDestinationIdentifier();
}

//----------------------------------------------------------------------------------------------------------------------

Message::Network::Type NetworkMessage::GetMessageType() const
{
	return m_type;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Buffer const& NetworkMessage::GetPayload() const
{
	return m_payload;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t NetworkMessage::GetPackSize() const
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
std::string NetworkMessage::GetPack() const
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

Message::ShareablePack NetworkMessage::GetShareablePack() const
{
	return std::make_shared<std::string const>(GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

Message::ValidationStatus NetworkMessage::Validate() const
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

constexpr std::size_t NetworkMessage::FixedPackSize() const
{
	std::size_t size = 0;
	size += sizeof(Message::Network::Type); // 1 byte for network message type
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += sizeof(std::uint8_t); // 1 byte for extensions size
    assert(std::in_range<std::uint32_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder::NetworkBuilder()
    : m_message()
	, m_hasStageFailure(false)
{
	m_message.m_header.m_protocol = Message::Protocol::Network;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetMessageContext(MessageContext const& context)
{
	m_message.m_context = context;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetSource(Node::Identifier const& identifier)
{
	m_message.m_header.m_source = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetSource(
    Node::Internal::Identifier::Type const& identifier)
{
	m_message.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetSource(std::string_view identifier)
{
	m_message.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetDestination(Node::Identifier const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetDestination(
    Node::Internal::Identifier::Type const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetDestination(std::string_view identifier)
{
	m_message.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::MakeHandshakeMessage()
{
	m_message.m_type = Message::Network::Type::Handshake;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::MakeHeartbeatRequest()
{
	m_message.m_type = Message::Network::Type::HeartbeatRequest;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::MakeHeartbeatResponse()
{
	m_message.m_type = Message::Network::Type::HeartbeatResponse;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetPayload(std::string_view buffer)
{
    return SetPayload({ reinterpret_cast<std::uint8_t const*>(buffer.data()), buffer.size() });
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::SetPayload(std::span<std::uint8_t const> buffer)
{
    m_message.m_payload = Message::Buffer(buffer.begin(), buffer.end());
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::FromDecodedPack(std::span<std::uint8_t const> buffer)
{
    if (!buffer.empty()) { Unpack(buffer); }
	else { m_hasStageFailure = true; }
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder& NetworkBuilder::FromEncodedPack(std::string_view pack)
{
    if (!pack.empty()) { Unpack(Z85::Decode(pack)); }
	else { m_hasStageFailure = true; }
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

NetworkMessage&& NetworkBuilder::Build()
{
	m_message.m_header.m_size = static_cast<std::uint32_t>(m_message.GetPackSize());

    return std::move(m_message);
}

//----------------------------------------------------------------------------------------------------------------------

NetworkBuilder::OptionalMessage NetworkBuilder::ValidatedBuild()
{
	m_message.m_header.m_size = static_cast<std::uint32_t>(m_message.GetPackSize());

    if (m_hasStageFailure || m_message.Validate() != Message::ValidationStatus::Success) { return {}; }
    return std::move(m_message);
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Unpack the raw message string into the Message class variables.
//----------------------------------------------------------------------------------------------------------------------
void NetworkBuilder::Unpack(std::span<std::uint8_t const> buffer)
{
	auto begin = buffer.begin();
	auto end = buffer.end();

	if (!m_message.m_header.ParseBuffer(begin, end)) {
		return;
	}

	m_message.m_type = local::UnpackMessageType(begin, end);
	if (m_message.m_type == Message::Network::Type::Invalid) { return; }

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_message.m_header.m_protocol != Message::Protocol::Network) { return; }

	std::uint32_t dataSize = 0;
	if (!PackUtils::UnpackChunk(begin, end, dataSize)) { return; }

	m_message.m_payload.reserve(dataSize);
	if (!PackUtils::UnpackChunk(begin, end, m_message.m_payload)) { return; }

	std::uint8_t extensions = 0;
	if (!PackUtils::UnpackChunk(begin, end, extensions)) { return; }

	if (extensions != 0) { UnpackExtensions(begin, end); }
}

//----------------------------------------------------------------------------------------------------------------------

void NetworkBuilder::UnpackExtensions(
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
