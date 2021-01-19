//------------------------------------------------------------------------------------------------
// File: ApplicationMessage.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ApplicationMessage.hpp"
#include "PackUtils.hpp"
#include "Utilities/Z85.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

Handler::Type UnpackCommand(
	std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end);

std::optional<Message::BoundTrackerKey> UnpackAwaitTracker(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end,
	std::uint32_t expected);

//------------------------------------------------------------------------------------------------
namespace Extensions {
//------------------------------------------------------------------------------------------------

enum Types : std::uint8_t { Invalid = 0x00, AwaitTracker = 0x01 };

//------------------------------------------------------------------------------------------------
} // Extensions namespace 
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//------------------------------------------------------------------------------------------------

ApplicationMessage::ApplicationMessage()
	: m_context()
	, m_header()
	, m_command()
	, m_phase()
	, m_payload()
	, m_timestamp(TimeUtils::GetSystemTimestamp())
	, m_optBoundAwaitTracker()
{
}

//------------------------------------------------------------------------------------------------

ApplicationMessage::ApplicationMessage(ApplicationMessage const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_command(other.m_command)
	, m_phase(other.m_phase)
	, m_payload(other.m_payload)
	, m_timestamp(other.m_timestamp)
	, m_optBoundAwaitTracker(other.m_optBoundAwaitTracker)
{
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder ApplicationMessage::Builder()
{
	return ApplicationBuilder{};
}

//------------------------------------------------------------------------------------------------

MessageContext const& ApplicationMessage::GetContext() const
{
	return m_context;
}

//------------------------------------------------------------------------------------------------

MessageHeader const& ApplicationMessage::GetMessageHeader() const
{
	return m_header;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::Container const& ApplicationMessage::GetSourceIdentifier() const
{
	return m_header.GetSourceIdentifier();
}

//------------------------------------------------------------------------------------------------

Message::Destination ApplicationMessage::GetDestinationType() const
{
	return m_header.GetDestinationType();
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::Container> const& ApplicationMessage::GetDestinationIdentifier() const
{
	return m_header.GetDestinationIdentifier();
}

//------------------------------------------------------------------------------------------------

Handler::Type ApplicationMessage::GetCommand() const
{
	return m_command;
}

//------------------------------------------------------------------------------------------------

std::uint8_t ApplicationMessage::GetPhase() const
{
	return m_phase;
}

//------------------------------------------------------------------------------------------------

Message::Buffer ApplicationMessage::GetPayload() const
{
	assert(m_context.HasSecurityHandlers());
	auto const data = m_context.Decrypt(m_payload, m_timestamp);
	if (!data) {
		return {};
	}

	return *data;
}

//------------------------------------------------------------------------------------------------

TimeUtils::Timestamp const& ApplicationMessage::GetTimestamp() const
{
	return m_timestamp;
}

//------------------------------------------------------------------------------------------------

std::optional<Await::TrackerKey> ApplicationMessage::GetAwaitTrackerKey() const
{
	if (!m_optBoundAwaitTracker) {
		return {};
	}
	return m_optBoundAwaitTracker->second;
}

//------------------------------------------------------------------------------------------------

std::uint32_t ApplicationMessage::GetPackSize() const
{
	std::size_t size = FixedPackSize();
	size += m_header.GetPackSize();
	size += m_payload.size();
	if (m_optBoundAwaitTracker) {
		size += FixedAwaitExtensionSize();
	}

	assert(m_context.HasSecurityHandlers());
	size += m_context.GetSignatureSize();

	auto const encoded = Z85::EncodedSize(size);
	assert(encoded < std::numeric_limits<std::uint32_t>::max());
	return static_cast<std::uint32_t>(encoded);
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pack the Message class values into a single raw string.
//------------------------------------------------------------------------------------------------
std::string ApplicationMessage::GetPack() const
{
	Message::Buffer buffer = m_header.GetPackedBuffer();
	buffer.reserve(m_header.GetMessageSize());
    // Application Pack Schema: 
    //  - Section 1 (1 byte): Command Type
    //  - Section 2 (1 bytes): Command Phase
    //  - Section 3 (4 bytes): Command Data Size
    //  - Section 4 (N bytes): Command Data
    //  - Section 5 (8 bytes): Message Timestamp (Milliseconds)
    //  - Section 6 (1 byte): Extenstions Count
    //      - Section 6.1 (1 byte): Extension Type      |   Extension Start
    //      - Section 6.2 (2 bytes): Extension Size     |
    //      - Section 6.3 (N bytes): Extension Data     |   Extension End
    //  - Section 7 (N bytse): Authentication Token (Strategy Specific)

	PackUtils::PackChunk(m_command, buffer);
	PackUtils::PackChunk(m_phase, buffer);
	PackUtils::PackChunk<std::uint32_t>(m_payload, buffer);
	PackUtils::PackChunk(m_timestamp, buffer);

	// Extension Packing
	PackUtils::PackChunk(std::uint8_t(0), buffer);
	auto& extensionCountByte = buffer.back();
	if (m_optBoundAwaitTracker) {
		++extensionCountByte;
		PackUtils::PackChunk(local::Extensions::AwaitTracker, buffer);
		PackUtils::PackChunk(FixedAwaitExtensionSize(), buffer);
		PackUtils::PackChunk(m_optBoundAwaitTracker->first, buffer);
		PackUtils::PackChunk(m_optBoundAwaitTracker->second, buffer);
	}
	
	// Calculate the number of bytes needed to pad to next 4 byte boundary
	auto const paddingBytesRequired = (4 - (buffer.size() & 3)) & 3;
	// Pad the message to ensure the encoding method doesn't add padding to the end of the message.
	Message::Buffer padding(paddingBytesRequired, 0);
	buffer.insert(buffer.end(), padding.begin(), padding.end());

	// Message Signing
	assert(m_context.HasSecurityHandlers());
	if (m_context.Sign(buffer) < 0) {
		return "";
	}

	return Z85::Encode(buffer);
}

//------------------------------------------------------------------------------------------------

Message::ValidationStatus ApplicationMessage::Validate() const
{	
	// A message must have a valid header
	if (!m_header.IsValid()) {
		return Message::ValidationStatus::Error;
	}

	// A message must identify a valid brypt command
	if (m_command == Handler::Type::Invalid) {
		return Message::ValidationStatus::Error;
	}

	// A message must identify the time it was created
	if (m_timestamp == TimeUtils::Timestamp()) {
		return Message::ValidationStatus::Error;
	}

	return Message::ValidationStatus::Success;
}

//------------------------------------------------------------------------------------------------

constexpr std::uint32_t ApplicationMessage::FixedPackSize() const
{
	std::size_t size = 0;
	size += sizeof(m_command); // 1 byte for command type
	size += sizeof(m_phase); // 1 byte for command phase
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += sizeof(std::uint64_t); // 8 bytes for message timestamp
	size += sizeof(std::uint8_t); // 1 byte for extensions size
	return static_cast<std::uint32_t>(size);
}

//------------------------------------------------------------------------------------------------

constexpr std::uint16_t ApplicationMessage::FixedAwaitExtensionSize() const
{
	std::size_t size = 0;
	size += sizeof(local::Extensions::AwaitTracker); // 1 byte for the extension type
	size += sizeof(std::uint16_t); // 2 bytes for the extension size
	size += sizeof(m_optBoundAwaitTracker->first); // 1 byte for await tracker binding
	size += sizeof(m_optBoundAwaitTracker->second); // 4 bytes for await tracker key
	return static_cast<std::uint16_t>(size);
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder::ApplicationBuilder()
    : m_message()
{
	m_message.m_header.m_protocol = Message::Protocol::Application;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetMessageContext(MessageContext const& context)
{
	m_message.m_context = context;
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetSource(BryptIdentifier::Container const& identifier)
{
	m_message.m_header.m_source = identifier;
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetSource(
    BryptIdentifier::Internal::Type const& identifier)
{
	m_message.m_header.m_source = BryptIdentifier::Container(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetSource(std::string_view identifier)
{
	m_message.m_header.m_source = BryptIdentifier::Container(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::MakeClusterMessage()
{
	m_message.m_header.m_destination = Message::Destination::Cluster;
	return *this;
}

//------------------------------------------------------------------------------------------------


ApplicationBuilder& ApplicationBuilder::MakeNetworkMessage()
{
	m_message.m_header.m_destination = Message::Destination::Network;
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetDestination(BryptIdentifier::Container const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetDestination(
    BryptIdentifier::Internal::Type const& identifier)
{
	m_message.m_header.m_optDestinationIdentifier = BryptIdentifier::Container(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetDestination(std::string_view identifier)
{
	m_message.m_header.m_optDestinationIdentifier = BryptIdentifier::Container(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetCommand(Handler::Type type, std::uint8_t phase)
{
	m_message.m_command = type;
	m_message.m_phase = phase;
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetPayload(std::string_view buffer)
{
    return SetPayload({ reinterpret_cast<std::uint8_t const*>(buffer.begin()), buffer.size() });
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::SetPayload(std::span<std::uint8_t const> buffer)
{
	assert(m_message.m_context.HasSecurityHandlers());
	auto const optData = m_message.m_context.Encrypt(buffer, m_message.m_timestamp);
	if(optData) {
		m_message.m_payload = *optData;
	}
    return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::BindAwaitTracker(
    Message::AwaitBinding binding,
    Await::TrackerKey key)
{
	m_message.m_optBoundAwaitTracker = Message::BoundTrackerKey(binding, key);
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::BindAwaitTracker(
    std::optional<Message::BoundTrackerKey> const& optBoundAwaitTracker)
{
	m_message.m_optBoundAwaitTracker = optBoundAwaitTracker;
	return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::FromDecodedPack(std::span<std::uint8_t const> buffer)
{
    if (buffer.empty()) {
        return *this;
    }

	assert(m_message.m_context.HasSecurityHandlers());
    if (m_message.m_context.Verify(buffer) != Security::VerificationStatus::Success) {
        return *this;
    }

	Unpack(buffer);
    return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder& ApplicationBuilder::FromEncodedPack(std::string_view pack)
{
    if (pack.empty()) {
        return *this;
    }
    
	auto const buffer = Z85::Decode(pack);
	assert(m_message.m_context.HasSecurityHandlers());
    if (m_message.m_context.Verify(buffer) != Security::VerificationStatus::Success) {
        return *this;
    }

    Unpack(buffer);
    return *this;
}

//------------------------------------------------------------------------------------------------

ApplicationMessage&& ApplicationBuilder::Build()
{
	m_message.m_header.m_size = m_message.GetPackSize();

    return std::move(m_message);
}

//------------------------------------------------------------------------------------------------

ApplicationBuilder::OptionalMessage ApplicationBuilder::ValidatedBuild()
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
void ApplicationBuilder::Unpack(std::span<std::uint8_t const> buffer)
{
	auto begin = buffer.begin();
	auto end = buffer.end();

	if (!m_message.m_header.ParseBuffer(begin, end)) {
		return;
	}

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_message.m_header.m_protocol != Message::Protocol::Application) {
		return;
	}

	if (m_message.m_command = local::UnpackCommand(begin, end);
		m_message.m_command == Handler::Type::Invalid) {
		return;
	}

	if (!PackUtils::UnpackChunk(begin, end, m_message.m_phase)) {
		return;
	}

	std::uint32_t dataSize = 0;
	if (!PackUtils::UnpackChunk(begin, end, dataSize)) {
		return;
	}

	m_message.m_payload.reserve(dataSize);
	if (!PackUtils::UnpackChunk(begin, end, m_message.m_payload)) {
		return;
	}

	std::uint64_t timestamp;
	if (!PackUtils::UnpackChunk(begin, end, timestamp)) {
		return;
	}
	m_message.m_timestamp = TimeUtils::Timestamp(timestamp);

	std::uint8_t extensions = 0;
	if (!PackUtils::UnpackChunk(begin, end, extensions)) {
		return;
	}

	if (extensions != 0) {
		UnpackExtensions(begin, end);
	}
}

//------------------------------------------------------------------------------------------------

void ApplicationBuilder::UnpackExtensions(
	std::span<std::uint8_t const>::iterator& begin,
	std::span<std::uint8_t const>::iterator const& end)
{
	while (begin != end) {
		using ExtensionType = std::underlying_type_t<local::Extensions::Types>;
		ExtensionType extension = 0;
		PackUtils::UnpackChunk(begin, end, extension);

		switch (extension)
		{
			case static_cast<ExtensionType>(local::Extensions::AwaitTracker): {
				m_message.m_optBoundAwaitTracker = local::UnpackAwaitTracker(
					begin, end, m_message.FixedAwaitExtensionSize());
			} break;					
			default: return;
		}
	}
}

//------------------------------------------------------------------------------------------------

Handler::Type local::UnpackCommand(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end)
{
    using HandlerType = std::underlying_type_t<Handler::Type>;
    HandlerType command = 0;
    PackUtils::UnpackChunk(begin, end, command);

    switch (command) {
        case static_cast<HandlerType>(Handler::Type::Connect):
        case static_cast<HandlerType>(Handler::Type::Election):
        case static_cast<HandlerType>(Handler::Type::Information):
        case static_cast<HandlerType>(Handler::Type::Query): {
            return static_cast<Handler::Type>(command);
        }
        default: return Handler::Type::Invalid;
    }
}

//------------------------------------------------------------------------------------------------

std::optional<Message::BoundTrackerKey> local::UnpackAwaitTracker(
    std::span<std::uint8_t const>::iterator& begin,
    std::span<std::uint8_t const>::iterator const& end,
	std::uint32_t expected)
{
	std::uint16_t size = 0;
	if (!PackUtils::UnpackChunk(begin, end, size)) {
		return {};
	}
	
	if (size < expected) {
		return {};
	}

	using BindingType = std::underlying_type_t<Message::AwaitBinding>;
	BindingType binding = 0;
	if (!PackUtils::UnpackChunk(begin, end, binding)) {
		return {};
	}

	Await::TrackerKey tracker = 0;
	if (!PackUtils::UnpackChunk(begin, end, tracker)) {
		return {};
	}

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

//------------------------------------------------------------------------------------------------
