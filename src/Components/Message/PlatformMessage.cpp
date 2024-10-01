//----------------------------------------------------------------------------------------------------------------------
// File: PlatformMessage.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "PlatformMessage.hpp"
#include "PackUtils.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

Message::Platform::ParcelType UnpackMessageType(
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

Message::Platform::Parcel::Parcel()
	: m_context()
	, m_header()
	, m_type(Message::Platform::ParcelType::Invalid)
	, m_payload()
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Parcel::Parcel(Parcel const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_type(other.m_type)
	, m_payload(other.m_payload)
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Parcel& Message::Platform::Parcel::operator=(Parcel const& other)
{
	m_context = other.m_context;
	m_header = other.m_header;
	m_type = other.m_type;
	m_payload = other.m_payload;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Platform::Parcel::operator==(Parcel const& other) const
{
	return 	m_context == other.m_context &&
			m_header == other.m_header &&
			m_type == other.m_type &&
			m_payload == other.m_payload;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder Message::Platform::Parcel::GetBuilder() { return Builder{}; }

//----------------------------------------------------------------------------------------------------------------------

Message::Context const& Message::Platform::Parcel::GetContext() const { return m_context; }

//----------------------------------------------------------------------------------------------------------------------

Message::Header const& Message::Platform::Parcel::GetHeader() const { return m_header; }

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& Message::Platform::Parcel::GetSource() const { return m_header.GetSource(); }

//----------------------------------------------------------------------------------------------------------------------

Message::Destination Message::Platform::Parcel::GetDestinationType() const { return m_header.GetDestinationType(); }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> const& Message::Platform::Parcel::GetDestination() const
{
	return m_header.GetDestination();
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::ParcelType Message::Platform::Parcel::GetType() const { return m_type; }

//----------------------------------------------------------------------------------------------------------------------

Message::Payload const& Message::Platform::Parcel::GetPayload() const { return m_payload; }

//----------------------------------------------------------------------------------------------------------------------

Message::Payload&& Message::Platform::Parcel::ExtractPayload() { return std::move(m_payload); }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Platform::Parcel::GetPackSize() const
{
	std::size_t size = FixedPackSize();
	size += m_header.GetPackSize();
	size += m_payload.GetPackSize();

	auto const encoded = Z85::EncodedSize(size);
    assert(std::in_range<std::uint32_t>(encoded));
	return encoded;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Pack the Message class values into a single raw string.
//----------------------------------------------------------------------------------------------------------------------
std::string Message::Platform::Parcel::GetPack() const
{
	Message::Buffer buffer = m_header.GetPackedBuffer();
	buffer.reserve(m_header.GetMessageSize());
    // Network Pack Schema: 
    //  - Section 1 (1 bytes): Network Message Type
    //  - Section 2 (4 bytes): Network Payload Size
    //  - Section 3 (N bytes): Network Payload
    //  - Section 4 (1 byte): Extensions Count
    //      - Section 4.1 (1 byte): Extension Type      |   Extension Start
    //      - Section 4.2 (2 bytes): Extension Size     |
    //      - Section 4.3 (N bytes): Extension Data     |   Extension End

	PackUtils::PackChunk(m_type, buffer);
	m_payload.Inject(buffer);

	// Extension Packing
	PackUtils::PackChunk(std::uint8_t(0), buffer);

	return Z85::Encode(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

Message::ShareablePack Message::Platform::Parcel::GetShareablePack() const
{
	return std::make_shared<std::string const>(GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

Message::ValidationStatus Message::Platform::Parcel::Validate() const
{	
	// A message must have a valid header
	if (!m_header.IsValid()) {
		return Message::ValidationStatus::Error;
	}

	// The network message type must not be invalid.
	if (m_type == Message::Platform::ParcelType::Invalid) { return Message::ValidationStatus::Error; }

	return Message::ValidationStatus::Success;
}

//----------------------------------------------------------------------------------------------------------------------

constexpr std::size_t Message::Platform::Parcel::FixedPackSize() const
{
	std::size_t size = 0;
	size += sizeof(Message::Platform::ParcelType); // 1 byte for network message type
	size += sizeof(std::uint8_t); // 1 byte for extensions size
    assert(std::in_range<std::uint32_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder::Builder()
    : m_parcel()
	, m_hasStageFailure(false)
{
	m_parcel.m_header.m_protocol = Message::Protocol::Platform;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& Message::Platform::Builder::GetSource() const
{
	return m_parcel.m_header.m_source;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> const& Message::Platform::Builder::GetDestination() const
{
	return m_parcel.m_header.m_optDestinationIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context const& Message::Platform::Builder::GetContext() const
{
	return m_parcel.m_context;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetContext(Context const& context)
{
	m_parcel.m_context = context;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetSource(Node::Identifier const& identifier)
{
	m_parcel.m_header.m_source = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetSource(
    Node::Internal::Identifier const& identifier)
{
	m_parcel.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetSource(std::string_view identifier)
{
	m_parcel.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetDestination(Node::Identifier const& identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetDestination(
    Node::Internal::Identifier const& identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetDestination(std::string_view identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::MakeHandshakeMessage()
{
	m_parcel.m_type = Message::Platform::ParcelType::Handshake;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::MakeHeartbeatRequest()
{
	m_parcel.m_type = Message::Platform::ParcelType::HeartbeatRequest;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::MakeHeartbeatResponse()
{
	m_parcel.m_type = Message::Platform::ParcelType::HeartbeatResponse;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetPayload(Payload&& payload)
{
	m_parcel.m_payload = std::move(payload);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::SetPayload(Payload const& payload)
{
	m_parcel.m_payload = Payload{ payload };
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::FromDecodedPack(std::span<std::uint8_t const> buffer)
{
	bool const success = !buffer.empty() && Unpack(buffer);
	if (!success) { m_hasStageFailure = true; }
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder& Message::Platform::Builder::FromEncodedPack(std::string_view pack)
{
	bool const success = !pack.empty() && Unpack(Z85::Decode(pack));
	if (!success) { m_hasStageFailure = true; }
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Parcel&& Message::Platform::Builder::Build()
{
	m_parcel.m_header.m_size = static_cast<std::uint32_t>(m_parcel.GetPackSize());

    return std::move(m_parcel);
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::Builder::OptionalParcel Message::Platform::Builder::ValidatedBuild()
{
	m_parcel.m_header.m_size = static_cast<std::uint32_t>(m_parcel.GetPackSize());

    if (m_hasStageFailure || m_parcel.Validate() != Message::ValidationStatus::Success) { return {}; }
    return std::move(m_parcel);
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Unpack the raw message string into the Message class variables.
//----------------------------------------------------------------------------------------------------------------------
bool Message::Platform::Builder::Unpack(std::span<std::uint8_t const> buffer)
{
	auto begin = buffer.begin();
	auto const end = buffer.end();

	if (!m_parcel.m_header.ParseBuffer(begin, end)) { return false; }

	m_parcel.m_type = local::UnpackMessageType(begin, end);
	if (m_parcel.m_type == Message::Platform::ParcelType::Invalid) { return false; }

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_parcel.m_header.m_protocol != Message::Protocol::Platform) { return false; }

	if (!m_parcel.m_payload.Unpack(begin, end)) { return false; }

	std::uint8_t extensions = 0;
	if (!PackUtils::UnpackChunk(begin, end, extensions)) { return false; }
	if (extensions != 0 && !UnpackExtensions(begin, end, extensions)) { return false; }

	return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Platform::Builder::UnpackExtensions(
	[[maybe_unused]] std::span<std::uint8_t const>::iterator& begin,
	[[maybe_unused]] std::span<std::uint8_t const>::iterator const& end,
	[[maybe_unused]] std::size_t extensions)
{
	//std::size_t unpacked = 0;	
	//while (begin != end && unpacked < extensions) {
	//	using ExtensionType = std::underlying_type_t<local::Extensions::Types>;
	//	ExtensionType extension = 0;
	//	PackUtils::UnpackChunk(begin, end, extension);

	//	switch (extension) {				
	//		default: return false;
	//	}

	//	++unpacked;
	//}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Message::Platform::Builder& Message::Platform::Builder::MakeClusterMessage<InvokeContext::Test>()
{
	m_parcel.m_header.m_destination = Message::Destination::Cluster;
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

template<>
Message::Platform::Builder& Message::Platform::Builder::MakeNetworkMessage<InvokeContext::Test>()
{
	m_parcel.m_header.m_destination = Message::Destination::Network;
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Platform::ParcelType local::UnpackMessageType(
	std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end)
{
	using namespace Message::Platform;

	using NetworkType = std::underlying_type_t<ParcelType>;
	NetworkType type = 0;
    PackUtils::UnpackChunk(begin, end, type);

    switch (type) {
        case static_cast<NetworkType>(ParcelType::Handshake):
        case static_cast<NetworkType>(ParcelType::HeartbeatRequest):
        case static_cast<NetworkType>(ParcelType::HeartbeatResponse): {
            return static_cast<ParcelType>(type);
        }
        default: return ParcelType::Invalid;
    }
}

//----------------------------------------------------------------------------------------------------------------------
