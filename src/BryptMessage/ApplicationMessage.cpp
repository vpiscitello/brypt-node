//----------------------------------------------------------------------------------------------------------------------
// File: ApplicationMessage.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "ApplicationMessage.hpp"
#include "PackUtils.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

std::optional<Message::BoundTrackerKey> UnpackAwaitTracker(
	Message::Buffer::const_iterator& begin, Message::Buffer::const_iterator const& end, std::size_t expected);

//----------------------------------------------------------------------------------------------------------------------
namespace Extensions {
//----------------------------------------------------------------------------------------------------------------------

enum Types : std::uint8_t { Invalid = 0x00, AwaitTracker = 0x01 };

//----------------------------------------------------------------------------------------------------------------------
} // Extensions namespace 
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

ApplicationMessage::ApplicationMessage()
	: m_context()
	, m_header()
	, m_route()
	, m_payload()
	, m_optBoundAwaitTracker()
{
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationMessage::ApplicationMessage(ApplicationMessage const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_route(other.m_route)
	, m_payload(other.m_payload)
	, m_optBoundAwaitTracker(other.m_optBoundAwaitTracker)
{
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder ApplicationMessage::Builder() { return ApplicationBuilder{}; }

//----------------------------------------------------------------------------------------------------------------------

MessageContext const& ApplicationMessage::GetContext() const { return m_context; }

//----------------------------------------------------------------------------------------------------------------------

MessageHeader const& ApplicationMessage::GetMessageHeader() const { return m_header; }

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& ApplicationMessage::GetSourceIdentifier() const { return m_header.GetSourceIdentifier(); }

//----------------------------------------------------------------------------------------------------------------------

Message::Destination ApplicationMessage::GetDestinationType() const { return m_header.GetDestinationType(); }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> const& ApplicationMessage::GetDestinationIdentifier() const
{
	return m_header.GetDestinationIdentifier();
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& ApplicationMessage::GetRoute() const { return m_route; }

//----------------------------------------------------------------------------------------------------------------------

Message::Buffer ApplicationMessage::GetPayload() const { return m_payload; }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Await::TrackerKey> ApplicationMessage::GetAwaitTrackerKey() const
{
	if (!m_optBoundAwaitTracker) { return {}; }
	return m_optBoundAwaitTracker->second;
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t ApplicationMessage::GetPackSize() const
{
	std::size_t size = FixedPackSize();
	size += m_header.GetPackSize();
	size += m_route.size();
	size += m_payload.size();
	if (m_optBoundAwaitTracker) { size += FixedAwaitExtensionSize(); }

	assert(m_context.HasSecurityHandlers());
	size += m_context.GetSignatureSize();

	auto const encoded = Z85::EncodedSize(size);
	assert(std::in_range<std::uint32_t>(encoded));
	return encoded;
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Pack the Message class values into a single raw string.
//----------------------------------------------------------------------------------------------------------------------
std::string ApplicationMessage::GetPack() const
{
    // Application Pack Schema: 
    //  - Section 1 (2 bytes): Route Size
    //  - Section 2 (N bytes): Route Data
    //  - Section 3 (4 bytes): Payload Size
    //  - Section 4 (N bytes): Payload Data
    //  - Section 5 (1 byte): Extenstions Count
    //      - Section 5.1 (1 byte): Extension Type      |   Extension Start
    //      - Section 5.2 (2 bytes): Extension Size     |
    //      - Section 5.3 (N bytes): Extension Data     |   Extension End
    //  - Section 6 (N bytse): Authentication Token (Strategy Specific)

	Message::Buffer buffer = m_header.GetPackedBuffer();
	buffer.reserve(m_header.GetMessageSize());

	{
		Message::Buffer plaintext;
		plaintext.reserve(m_header.GetPackSize());
		PackUtils::PackChunk<std::uint16_t>(m_route, plaintext);
		PackUtils::PackChunk<std::uint32_t>(m_payload, plaintext);

		// Extension Packing
		PackUtils::PackChunk(std::uint8_t(0), plaintext);
		auto& extensions = plaintext.back();
		if (m_optBoundAwaitTracker) {
			++extensions;
			std::uint16_t const size = static_cast<std::uint16_t>(FixedAwaitExtensionSize());
			PackUtils::PackChunk(local::Extensions::AwaitTracker, plaintext);
			PackUtils::PackChunk(size, plaintext);
			PackUtils::PackChunk(m_optBoundAwaitTracker->first, plaintext);
			PackUtils::PackChunk(m_optBoundAwaitTracker->second, plaintext);
		}

		auto optEncryptedBuffer = m_context.Encrypt(plaintext, m_header.GetTimestamp());
		if (!optEncryptedBuffer) { return {}; }

		std::ranges::move(*optEncryptedBuffer, std::back_inserter(buffer));
	}
	
	// Calculate the number of bytes needed to pad to next 4 byte boundary
	std::size_t const pad = (4 - (buffer.size() & 3)) & 3;
	// Pad the message to ensure the encoding method doesn't add padding to the end of the message.
	Message::Buffer padding(pad, 0);
	buffer.insert(buffer.end(), padding.begin(), padding.end());

	// Message Signing
	assert(m_context.HasSecurityHandlers());
	if (m_context.Sign(buffer) < 0) { return {}; }

	return Z85::Encode(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

Message::ShareablePack ApplicationMessage::GetShareablePack() const
{
	return std::make_shared<std::string const>(GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

Message::ValidationStatus ApplicationMessage::Validate() const
{
	using enum Message::ValidationStatus;
	if (!m_header.IsValid()) { return Error; } // A message must have a valid header
	if (m_route.empty()) { return Error; } // A message must identify a valid route
	return Success;
}

//----------------------------------------------------------------------------------------------------------------------

constexpr std::size_t ApplicationMessage::FixedPackSize() const
{
	std::size_t size = 0;
	size += sizeof(std::uint16_t); // 2 byte for route size
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += sizeof(std::uint8_t); // 1 byte for extensions size
    assert(std::in_range<std::uint32_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

constexpr std::size_t ApplicationMessage::FixedAwaitExtensionSize() const
{
	std::size_t size = 0;
	size += sizeof(local::Extensions::AwaitTracker); // 1 byte for the extension type
	size += sizeof(std::uint16_t); // 2 bytes for the extension size
	size += sizeof(m_optBoundAwaitTracker->first); // 1 byte for await tracker binding
	size += sizeof(m_optBoundAwaitTracker->second); // 4 bytes for await tracker key
    assert(std::in_range<std::uint16_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder::ApplicationBuilder()
    : m_message()
	, m_hasStageFailure(false)
{
	m_message.m_header.m_protocol = Message::Protocol::Application;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetMessageContext(MessageContext const& context)
{
	m_message.m_context = context;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetSource(Node::Identifier const& identifier)
{
	m_message.m_header.m_source = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetSource(
    Node::Internal::Identifier const& identifier)
{
	m_message.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetSource(std::string_view identifier)
{
	m_message.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::MakeClusterMessage()
{
	m_message.m_header.m_destination = Message::Destination::Cluster;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------


ApplicationBuilder& ApplicationBuilder::MakeNetworkMessage()
{
	m_message.m_header.m_destination = Message::Destination::Network;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetDestination(Node::Identifier const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetDestination(
    Node::Internal::Identifier const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetDestination(std::string_view identifier)
{
	m_message.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetRoute(std::string_view const& route)
{
	m_message.m_route = route;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetRoute(std::string&& route)
{
	m_message.m_route = std::move(route);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetPayload(std::string_view buffer)
{
    return SetPayload({ reinterpret_cast<std::uint8_t const*>(buffer.begin()), buffer.size() });
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetPayload(std::span<std::uint8_t const> buffer)
{
	assert(m_message.m_context.HasSecurityHandlers()); // TODO: Move this somewhere
    return SetPayload(Message::Buffer{ buffer.begin(), buffer.end() });
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetPayload(Message::Buffer&& buffer)
{
	m_message.m_payload = std::move(buffer);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::BindAwaitTracker(
    Message::AwaitBinding binding,
    Await::TrackerKey key)
{
	m_message.m_optBoundAwaitTracker = Message::BoundTrackerKey(binding, key);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::BindAwaitTracker(
    std::optional<Message::BoundTrackerKey> const& optBoundAwaitTracker)
{
	m_message.m_optBoundAwaitTracker = optBoundAwaitTracker;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::FromDecodedPack(std::span<std::uint8_t const> buffer)
{
    if (buffer.empty()) { return *this; }

	assert(m_message.m_context.HasSecurityHandlers());
    if (m_message.m_context.Verify(buffer) == Security::VerificationStatus::Success) { Unpack(buffer); }
	else { m_hasStageFailure = true; }

    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::FromEncodedPack(std::string_view pack)
{
    if (pack.empty()) { m_hasStageFailure = true; return *this; }
    
	auto const buffer = Z85::Decode(pack);
	assert(m_message.m_context.HasSecurityHandlers());
    if (m_message.m_context.Verify(buffer) == Security::VerificationStatus::Success) { Unpack(buffer); }
	else { m_hasStageFailure = true; }

    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationMessage&& ApplicationBuilder::Build()
{
	m_message.m_header.m_size = static_cast<std::uint32_t>(m_message.GetPackSize()); // Set the estimated message size.
    return std::move(m_message);
}

//----------------------------------------------------------------------------------------------------------------------

ApplicationBuilder::OptionalMessage ApplicationBuilder::ValidatedBuild()
{
	m_message.m_header.m_size = static_cast<std::uint32_t>(m_message.GetPackSize()); // Set the estimated message size.
    if (m_hasStageFailure || m_message.Validate() != Message::ValidationStatus::Success) { return {}; }
    return std::move(m_message);
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Unpack the raw message string into the Message class variables.
//----------------------------------------------------------------------------------------------------------------------
void ApplicationBuilder::Unpack(std::span<std::uint8_t const> buffer)
{
	{
		auto begin = buffer.begin();
		auto end = buffer.end();
		if (!m_message.m_header.ParseBuffer(begin, end)) { return; }
	}

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_message.m_header.m_protocol != Message::Protocol::Application) { return; }

	// Create a view of the encrypted portion of the application message. This will be from the end of the header to 
	// the beginning of the signature. 
	Security::ReadableView bufferview = { 
		buffer.begin() + m_message.m_header.GetPackSize(),
		buffer.size() - m_message.m_header.GetPackSize() - m_message.m_context.GetSignatureSize() };

	auto const optDecryptedBuffer = m_message.m_context.Decrypt(bufferview, m_message.m_header.GetTimestamp());
	if (!optDecryptedBuffer) { return; }

	auto begin = optDecryptedBuffer->begin();
	auto end = optDecryptedBuffer->end();
	{
		std::uint16_t size = 0;
		if (!PackUtils::UnpackChunk(begin, end, size)) { return; }
		if (!PackUtils::UnpackChunk(begin, end, m_message.m_route, size)) { return; }
	}
 
	{
		std::uint32_t size = 0;
		if (!PackUtils::UnpackChunk(begin, end, size)) { return; }
		if (!PackUtils::UnpackChunk(begin, end, m_message.m_payload, size)) { return; }
	}

	std::uint8_t extensions = 0;
	if (!PackUtils::UnpackChunk(begin, end, extensions)) { return; }

	if (extensions != 0) { UnpackExtensions(begin, end); }
}

//----------------------------------------------------------------------------------------------------------------------

void ApplicationBuilder::UnpackExtensions(
	Message::Buffer::const_iterator& begin, Message::Buffer::const_iterator const& end)
{
	while (begin != end) {
		using ExtensionType = std::underlying_type_t<local::Extensions::Types>;
		ExtensionType extension = 0;
		PackUtils::UnpackChunk(begin, end, extension);

		switch (extension){
			case static_cast<ExtensionType>(local::Extensions::AwaitTracker): {
				m_message.m_optBoundAwaitTracker = local::UnpackAwaitTracker(
					begin, end, m_message.FixedAwaitExtensionSize());
			} break;					
			default: return;
		}
	}
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Message::BoundTrackerKey> local::UnpackAwaitTracker(
	Message::Buffer::const_iterator& begin, Message::Buffer::const_iterator const& end, std::size_t expected)
{
	std::uint16_t size = 0;
	if (!PackUtils::UnpackChunk(begin, end, size)) { return {}; }
	
	if (std::cmp_less(size, expected)) { return {}; }

	using BindingType = std::underlying_type_t<Message::AwaitBinding>;
	BindingType binding = 0;
	if (!PackUtils::UnpackChunk(begin, end, binding)) { return {}; }

	Await::TrackerKey tracker = 0;
	if (!PackUtils::UnpackChunk(begin, end, tracker)) { return {}; }

	switch (binding) {
		case static_cast<BindingType>(Message::AwaitBinding::Source): {
			return Message::BoundTrackerKey{ Message::AwaitBinding::Source, tracker };
		}
		case static_cast<BindingType>(Message::AwaitBinding::Destination): {
			return Message::BoundTrackerKey{ Message::AwaitBinding::Destination, tracker };
		}
		default: return {};
	}
}

//----------------------------------------------------------------------------------------------------------------------
