//------------------------------------------------------------------------------------------------
// File: HandshakeMessage.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "HandshakeMessage.hpp"
#include "MessageSecurity.hpp"
#include "PackUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

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

CHandshakeMessage::CHandshakeMessage()
	: m_context()
	, m_header()
	, m_data()
{
}

//------------------------------------------------------------------------------------------------

CHandshakeMessage::CHandshakeMessage(CHandshakeMessage const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_data(other.m_data)
{
}
//------------------------------------------------------------------------------------------------

CHandshakeBuilder CHandshakeMessage::Builder()
{
	return CHandshakeBuilder{};
}

//------------------------------------------------------------------------------------------------

CMessageContext const& CHandshakeMessage::GetMessageContext() const
{
	return m_context;
}

//------------------------------------------------------------------------------------------------

CMessageHeader const& CHandshakeMessage::GetMessageHeader() const
{
	return m_header;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const& CHandshakeMessage::GetSourceIdentifier() const
{
	return m_header.GetSourceIdentifier();
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::CContainer> const& CHandshakeMessage::GetDestinationIdentifier() const
{
	return m_header.GetDestinationIdentifier();
}

//------------------------------------------------------------------------------------------------

Message::Buffer CHandshakeMessage::GetData() const
{
	return m_data;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CHandshakeMessage::GetPackSize() const
{
	std::uint32_t size = FixedPackSize();
	size += m_header.GetPackSize();
	size += m_data.size();

	// Account for the ASCII encoding method
	size += (4 - (size & 3)) & 3;
	size *= PackUtils::Z85Multiplier;

	return size;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pack the Message class values into a single raw string.
//------------------------------------------------------------------------------------------------
std::string CHandshakeMessage::GetPack() const
{
	Message::Buffer buffer = m_header.GetPackedBuffer();
	buffer.reserve(GetPackSize());
    // Handshake Pack Schema: 
    //  - Section 1 (4 bytes): Handshake Data Size
    //  - Section 2 (N bytes): Handshake Data
    //  - Section 3 (1 byte): Extenstions Count
    //      - Section 3.1 (1 byte): Extension Type      |   Extension Start
    //      - Section 3.2 (2 bytes): Extension Size     |
    //      - Section 3.3 (N bytes): Extension Data     |   Extension End

	PackUtils::PackChunk(buffer, m_data, sizeof(std::uint32_t));

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

Message::ValidationStatus CHandshakeMessage::Validate() const
{	
	// A message must have a valid header
	if (!m_header.IsValid()) {
		return Message::ValidationStatus::Error;
	}

	return Message::ValidationStatus::Success;
}

//------------------------------------------------------------------------------------------------

constexpr std::uint32_t CHandshakeMessage::FixedPackSize() const
{
	std::uint32_t size = 0;
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += sizeof(std::uint8_t); // 1 byte for extensions size
	return size;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder::CHandshakeBuilder()
    : m_message()
{
	m_message.m_header.m_protocol = Message::Protocol::Handshake;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetMessageContext(CMessageContext const& context)
{
	m_message.m_context = context;
	return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetSource(BryptIdentifier::CContainer const& identifier)
{
	m_message.m_header.m_source = identifier;
	return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetSource(
    BryptIdentifier::Internal::Type const& identifier)
{
	m_message.m_header.m_source = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetSource(std::string_view identifier)
{
	m_message.m_header.m_source = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetDestination(BryptIdentifier::CContainer const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetDestination(
    BryptIdentifier::Internal::Type const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetDestination(std::string_view identifier)
{
	m_message.m_header.m_optDestinationIdentifier = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetData(std::string_view data)
{
    return SetData({ data.begin(), data.end() });
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::SetData(Message::Buffer const& buffer)
{
    m_message.m_data = buffer;
    return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::FromDecodedPack(Message::Buffer const& buffer)
{
    if (buffer.empty()) {
        return *this;
    }

	Unpack(buffer);
    
    return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder& CHandshakeBuilder::FromEncodedPack(std::string_view pack)
{
    if (pack.empty()) {
        return *this;
    }

	Unpack(PackUtils::Z85Decode(pack));
    
    return *this;
}

//------------------------------------------------------------------------------------------------

CHandshakeMessage&& CHandshakeBuilder::Build()
{
    return std::move(m_message);
}

//------------------------------------------------------------------------------------------------

CHandshakeBuilder::OptionalMessage CHandshakeBuilder::ValidatedBuild()
{
    if (m_message.Validate() != Message::ValidationStatus::Success) {
        return {};
    }
    return std::move(m_message);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Unpack the raw message string into the Message class variables.
//------------------------------------------------------------------------------------------------
void CHandshakeBuilder::Unpack(Message::Buffer const& buffer)
{
	Message::Buffer::const_iterator begin = buffer.begin();
	Message::Buffer::const_iterator end = buffer.end();

	if (!m_message.m_header.ParseBuffer(begin, end)) {
		return;
	}

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_message.m_header.m_protocol != Message::Protocol::Handshake) {
		return;
	}

	std::uint32_t dataSize = 0;
	if (!PackUtils::UnpackChunk(begin, end, dataSize)) {
		return;
	}

	if (!PackUtils::UnpackChunk(begin, end, m_message.m_data, dataSize)) {
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

void CHandshakeBuilder::UnpackExtensions(
	Message::Buffer::const_iterator& begin,
	Message::Buffer::const_iterator const& end)
{
	while (begin != end) {
		using ExtensionType = std::underlying_type_t<local::Extensions::Types>;
		ExtensionType extension = 0;
		PackUtils::UnpackChunk(begin, end, extension);

		switch (extension)
		{				
			default: return;
		}
	}
}

//------------------------------------------------------------------------------------------------
