//------------------------------------------------------------------------------------------------
// File: NetworkMessage.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "NetworkMessage.hpp"
#include "PackUtils.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

Message::Network::Type UnpackMessageType(
	Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end);

//------------------------------------------------------------------------------------------------
namespace Extensions {
//------------------------------------------------------------------------------------------------

enum Types : std::uint8_t { Invalid = 0x00 };

//------------------------------------------------------------------------------------------------
} // Extensions namespace 
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

CNetworkMessage::CNetworkMessage()
	: m_context()
	, m_header()
	, m_type(Message::Network::Type::Invalid)
	, m_payload()
{
}

//------------------------------------------------------------------------------------------------

CNetworkMessage::CNetworkMessage(CNetworkMessage const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_type(other.m_type)
	, m_payload(other.m_payload)
{
}
//------------------------------------------------------------------------------------------------

CNetworkBuilder CNetworkMessage::Builder()
{
	return CNetworkBuilder{};
}

//------------------------------------------------------------------------------------------------

CMessageContext const& CNetworkMessage::GetContext() const
{
	return m_context;
}

//------------------------------------------------------------------------------------------------

CMessageHeader const& CNetworkMessage::GetMessageHeader() const
{
	return m_header;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const& CNetworkMessage::GetSourceIdentifier() const
{
	return m_header.GetSourceIdentifier();
}

//------------------------------------------------------------------------------------------------

Message::Destination CNetworkMessage::GetDestinationType() const
{
	return m_header.GetDestinationType();
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::CContainer> const& CNetworkMessage::GetDestinationIdentifier() const
{
	return m_header.GetDestinationIdentifier();
}

//------------------------------------------------------------------------------------------------

Message::Network::Type CNetworkMessage::GetMessageType() const
{
	return m_type;
}

//------------------------------------------------------------------------------------------------

Message::Buffer const& CNetworkMessage::GetPayload() const
{
	return m_payload;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CNetworkMessage::GetPackSize() const
{
	std::uint32_t size = FixedPackSize();
	size += m_header.GetPackSize();
	size += m_payload.size();

	// Account for the ASCII encoding method
	size += (4 - (size & 3)) & 3;
	size *= PackUtils::Z85Multiplier;

	return size;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pack the Message class values into a single raw string.
//------------------------------------------------------------------------------------------------
std::string CNetworkMessage::GetPack() const
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

	PackUtils::PackChunk(buffer, m_type);
	PackUtils::PackChunk(buffer, m_payload, sizeof(std::uint32_t));

	// Extension Packing
	PackUtils::PackChunk(buffer, std::uint8_t(0));

	// Calculate the number of bytes needed to pad to next 4 byte boundary
	auto const paddingBytesRequired = (4 - (buffer.size() & 3)) & 3;
	// Pad the message to ensure the encoding method doesn't add padding to the end of the message.
	Message::Buffer padding(paddingBytesRequired, 0);
	buffer.insert(buffer.end(), padding.begin(), padding.end());

	return PackUtils::Z85Encode(buffer);
}

//------------------------------------------------------------------------------------------------

Message::ValidationStatus CNetworkMessage::Validate() const
{	
	// A message must have a valid header
	if (!m_header.IsValid()) {
		return Message::ValidationStatus::Error;
	}

	// The network message type must not be invalid.
	if (m_type == Message::Network::Type::Invalid) {
		return Message::ValidationStatus::Error;
	}

	return Message::ValidationStatus::Success;
}

//------------------------------------------------------------------------------------------------

constexpr std::uint32_t CNetworkMessage::FixedPackSize() const
{
	std::uint32_t size = 0;
	size += sizeof(Message::Network::Type); // 1 byte for network message type
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += sizeof(std::uint8_t); // 1 byte for extensions size
	return size;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder::CNetworkBuilder()
    : m_message()
{
	m_message.m_header.m_protocol = Message::Protocol::Network;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetMessageContext(CMessageContext const& context)
{
	m_message.m_context = context;
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetSource(BryptIdentifier::CContainer const& identifier)
{
	m_message.m_header.m_source = identifier;
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetSource(
    BryptIdentifier::Internal::Type const& identifier)
{
	m_message.m_header.m_source = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetSource(std::string_view identifier)
{
	m_message.m_header.m_source = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetDestination(BryptIdentifier::CContainer const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetDestination(
    BryptIdentifier::Internal::Type const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetDestination(std::string_view identifier)
{
	m_message.m_header.m_optDestinationIdentifier = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::MakeHandshakeMessage()
{
	m_message.m_type = Message::Network::Type::Handshake;
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::MakeHeartbeatRequest()
{
	m_message.m_type = Message::Network::Type::HeartbeatRequest;
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::MakeHeartbeatResponse()
{
	m_message.m_type = Message::Network::Type::HeartbeatResponse;
	return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetPayload(std::string_view data)
{
    return SetPayload({ data.begin(), data.end() });
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::SetPayload(Message::Buffer const& buffer)
{
    m_message.m_payload = buffer;
    return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::FromDecodedPack(Message::Buffer const& buffer)
{
    if (buffer.empty()) {
        return *this;
    }

	Unpack(buffer);
    
    return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder& CNetworkBuilder::FromEncodedPack(std::string_view pack)
{
    if (pack.empty()) {
        return *this;
    }

	Unpack(PackUtils::Z85Decode(pack));
    
    return *this;
}

//------------------------------------------------------------------------------------------------

CNetworkMessage&& CNetworkBuilder::Build()
{
	m_message.m_header.m_size = m_message.GetPackSize();

    return std::move(m_message);
}

//------------------------------------------------------------------------------------------------

CNetworkBuilder::OptionalMessage CNetworkBuilder::ValidatedBuild()
{
	m_message.m_header.m_size = m_message.GetPackSize();

    if (m_message.Validate() != Message::ValidationStatus::Success) {
        return {};
    }
    return std::move(m_message);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Unpack the raw message string into the Message class variables.
//------------------------------------------------------------------------------------------------
void CNetworkBuilder::Unpack(Message::Buffer const& buffer)
{
	Message::Buffer::const_iterator begin = buffer.begin();
	Message::Buffer::const_iterator end = buffer.end();

	if (!m_message.m_header.ParseBuffer(begin, end)) {
		return;
	}

	m_message.m_type = local::UnpackMessageType(begin, end);
	if (m_message.m_type == Message::Network::Type::Invalid) {
		return;
	}

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_message.m_header.m_protocol != Message::Protocol::Network) {
		return;
	}

	std::uint32_t dataSize = 0;
	if (!PackUtils::UnpackChunk(begin, end, dataSize)) {
		return;
	}

	if (!PackUtils::UnpackChunk(begin, end, m_message.m_payload, dataSize)) {
		return;
	}

	std::uint8_t extensions = 0;
	if (!PackUtils::UnpackChunk(begin, end, extensions)) {
		return;
	}

	if (extensions != 0) {
		UnpackExtensions(begin, end);
	}
}

//------------------------------------------------------------------------------------------------

void CNetworkBuilder::UnpackExtensions(
	Message::Buffer::const_iterator& begin,
	Message::Buffer::const_iterator const& end)
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

//------------------------------------------------------------------------------------------------

Message::Network::Type local::UnpackMessageType(
	Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end)
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

//------------------------------------------------------------------------------------------------
