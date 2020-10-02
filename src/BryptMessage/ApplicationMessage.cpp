//------------------------------------------------------------------------------------------------
// File: ApplicationMessage.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "ApplicationMessage.hpp"
#include "MessageSecurity.hpp"
#include "PackUtils.hpp"
#include "../Utilities/NodeUtils.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace {
namespace local {
//------------------------------------------------------------------------------------------------

Command::Type UnpackCommand(
	Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end);

std::optional<Message::BoundTrackerKey> UnpackAwaitTracker(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end,
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

CApplicationMessage::CApplicationMessage()
	: m_context()
	, m_header()
	, m_command()
	, m_phase()
	, m_data()
	, m_timestamp(TimeUtils::GetSystemTimestamp())
	, m_optBoundAwaitTracker()
{
}

//------------------------------------------------------------------------------------------------

CApplicationMessage::CApplicationMessage(CApplicationMessage const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_command(other.m_command)
	, m_phase(other.m_phase)
	, m_data(other.m_data)
	, m_timestamp(other.m_timestamp)
	, m_optBoundAwaitTracker(other.m_optBoundAwaitTracker)
{
}
//------------------------------------------------------------------------------------------------

CApplicationBuilder CApplicationMessage::Builder()
{
	return CApplicationBuilder{};
}

//------------------------------------------------------------------------------------------------

CMessageContext const& CApplicationMessage::GetMessageContext() const
{
	return m_context;
}

//------------------------------------------------------------------------------------------------

CMessageHeader const& CApplicationMessage::GetMessageHeader() const
{
	return m_header;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const& CApplicationMessage::GetSourceIdentifier() const
{
	return m_header.GetSourceIdentifier();
}

//------------------------------------------------------------------------------------------------

std::optional<BryptIdentifier::CContainer> const& CApplicationMessage::GetDestinationIdentifier() const
{
	return m_header.GetDestinationIdentifier();
}

//------------------------------------------------------------------------------------------------

Command::Type CApplicationMessage::GetCommand() const
{
	return m_command;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CApplicationMessage::GetPhase() const
{
	return m_phase;
}

//------------------------------------------------------------------------------------------------

Message::Buffer CApplicationMessage::GetData() const
{
	auto const data = MessageSecurity::Decrypt(m_data, m_data.size(), m_timestamp.count());

	if (!data) {
		return {};
	}

	return *data;
}

//------------------------------------------------------------------------------------------------

TimeUtils::Timestamp const& CApplicationMessage::GetTimestamp() const
{
	return m_timestamp;
}

//------------------------------------------------------------------------------------------------

std::optional<Await::TrackerKey> CApplicationMessage::GetAwaitTrackerKey() const
{
	if (!m_optBoundAwaitTracker) {
		return {};
	}
	return m_optBoundAwaitTracker->second;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CApplicationMessage::GetPackSize() const
{
	std::uint32_t size = FixedPackSize();
	size += m_header.GetPackSize();
	size += m_data.size();
	if (m_optBoundAwaitTracker) {
		size += FixedAwaitExtensionSize();
	}

	// Account for the ASCII encoding method
	size += (4 - (size & 3)) & 3;
	size *= PackUtils::Z85Multiplier;

	return size;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pack the Message class values into a single raw string.
//------------------------------------------------------------------------------------------------
std::string CApplicationMessage::GetPack() const
{
	Message::Buffer buffer = m_header.GetPackedBuffer();
	buffer.reserve(GetPackSize());
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

	PackUtils::PackChunk(buffer, m_command);
	PackUtils::PackChunk(buffer, m_phase);
	PackUtils::PackChunk(buffer, m_data, sizeof(std::uint32_t));
	PackUtils::PackChunk(buffer, m_timestamp);

	// Extension Packing
	PackUtils::PackChunk(buffer, std::uint8_t(0));
	auto& extensionCountByte = buffer.back();
	if (m_optBoundAwaitTracker) {
		++extensionCountByte;
		PackUtils::PackChunk(buffer, local::Extensions::AwaitTracker);
		PackUtils::PackChunk(buffer, FixedAwaitExtensionSize());
		PackUtils::PackChunk(buffer, m_optBoundAwaitTracker->first);
		PackUtils::PackChunk(buffer, m_optBoundAwaitTracker->second);
	}
	
	// Calculate the number of bytes needed to pad to next 4 byte boundary
	auto const paddingBytesRequired = (4 - (buffer.size() & 3)) & 3;
	// Pad the message to ensure the encoding method doesn't add padding to the end of the message.
	Message::Buffer padding(paddingBytesRequired, 0);
	buffer.insert(buffer.end(), padding.begin(), padding.end());

	// Signature Packing
	auto const optSignature = MessageSecurity::HMAC(buffer, buffer.size());
	if (!optSignature) {
		return "";
	}

	buffer.insert(buffer.end(), optSignature->begin(), optSignature->end());
	return PackUtils::Z85Encode(buffer);
}

//------------------------------------------------------------------------------------------------

Message::ValidationStatus CApplicationMessage::Validate() const
{	
	// A message must have a valid header
	if (!m_header.IsValid()) {
		return Message::ValidationStatus::Error;
	}

	// A message must identify a valid brypt command
	if (m_command == static_cast<Command::Type>(Command::Type::Invalid)) {
		return Message::ValidationStatus::Error;
	}

	// A message must identify the time it was created
	if (m_timestamp == TimeUtils::Timestamp()) {
		return Message::ValidationStatus::Error;
	}

	return Message::ValidationStatus::Success;
}

//------------------------------------------------------------------------------------------------

constexpr std::uint32_t CApplicationMessage::FixedPackSize() const
{
	std::uint32_t size = 0;
	size += sizeof(m_command); // 1 byte for command type
	size += sizeof(m_phase); // 1 byte for command phase
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += sizeof(std::uint64_t); // 8 bytes for message timestamp
	size += sizeof(std::uint8_t); // 1 byte for extensions size
	size += MessageSecurity::TokenSize; // N bytes for message token
	return size;
}

//------------------------------------------------------------------------------------------------

constexpr std::uint16_t CApplicationMessage::FixedAwaitExtensionSize() const
{
	std::uint16_t size = 0;
	size += sizeof(local::Extensions::AwaitTracker); // 1 byte for the extension type
	size += sizeof(std::uint16_t); // 2 bytes for the extension size
	size += sizeof(m_optBoundAwaitTracker->first); // 1 byte for await tracker binding
	size += sizeof(m_optBoundAwaitTracker->second); // 4 bytes for await tracker key
	return size;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder::CApplicationBuilder()
    : m_message()
{
	m_message.m_header.m_protocol = Message::Protocol::Application;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetMessageContext(CMessageContext const& context)
{
	m_message.m_context = context;
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetSource(BryptIdentifier::CContainer const& identifier)
{
	m_message.m_header.m_source = identifier;
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetSource(
    BryptIdentifier::Internal::Type const& identifier)
{
	m_message.m_header.m_source = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetSource(std::string_view identifier)
{
	m_message.m_header.m_source = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::MakeClusterMessage()
{
	m_message.m_header.m_destination = Message::Destination::Cluster;
	return *this;
}

//------------------------------------------------------------------------------------------------


CApplicationBuilder& CApplicationBuilder::MakeNetworkMessage()
{
	m_message.m_header.m_destination = Message::Destination::Network;
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetDestination(BryptIdentifier::CContainer const& identifier)
{
	m_message.m_header.m_destination = Message::Destination::Node;
	m_message.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetDestination(
    BryptIdentifier::Internal::Type const& identifier)
{
	m_message.m_header.m_destination = Message::Destination::Node;
	m_message.m_header.m_optDestinationIdentifier = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetDestination(std::string_view identifier)
{
	m_message.m_header.m_destination = Message::Destination::Node;
	m_message.m_header.m_optDestinationIdentifier = BryptIdentifier::CContainer(identifier);
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetCommand(Command::Type type, std::uint8_t phase)
{
	m_message.m_command = type;
	m_message.m_phase = phase;
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetData(std::string_view data)
{
    return SetData({ data.begin(), data.end() });
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::SetData(Message::Buffer const& buffer)
{
	auto const optData = MessageSecurity::Encrypt(
        buffer, buffer.size(), m_message.m_timestamp.count());
	if(optData) {
		m_message.m_data = *optData;
	}
    return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::BindAwaitTracker(
    Message::AwaitBinding binding,
    Await::TrackerKey key)
{
	m_message.m_optBoundAwaitTracker = Message::BoundTrackerKey(binding, key);
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::BindAwaitTracker(
    std::optional<Message::BoundTrackerKey> const& optBoundAwaitTracker)
{
	m_message.m_optBoundAwaitTracker = optBoundAwaitTracker;
	return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::FromDecodedPack(Message::Buffer const& buffer)
{
    if (buffer.empty()) {
        return *this;
    }

    if (MessageSecurity::Verify(buffer) != MessageSecurity::VerificationStatus::Success) {
        return *this;
    }

	Unpack(buffer);
    return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder& CApplicationBuilder::FromEncodedPack(std::string_view pack)
{
    if (pack.empty()) {
        return *this;
    }
    
	auto const buffer = PackUtils::Z85Decode(pack);
    if (MessageSecurity::Verify(buffer) != MessageSecurity::VerificationStatus::Success) {
        return *this;
    }

    Unpack(buffer);
    return *this;
}

//------------------------------------------------------------------------------------------------

CApplicationMessage&& CApplicationBuilder::Build()
{
    return std::move(m_message);
}

//------------------------------------------------------------------------------------------------

CApplicationBuilder::OptionalMessage CApplicationBuilder::ValidatedBuild()
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
void CApplicationBuilder::Unpack(Message::Buffer const& buffer)
{
	Message::Buffer::const_iterator begin = buffer.begin();
	Message::Buffer::const_iterator end = buffer.end();

	if (!m_message.m_header.ParseBuffer(begin, end)) {
		return;
	}

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_message.m_header.m_protocol != Message::Protocol::Application) {
		return;
	}

	if (m_message.m_command = local::UnpackCommand(begin, end);
		m_message.m_command == Command::Type::Invalid) {
		return;
	}

	if (!PackUtils::UnpackChunk(begin, end, m_message.m_phase)) {
		return;
	}

	std::uint32_t dataSize = 0;
	if (!PackUtils::UnpackChunk(begin, end, dataSize)) {
		return;
	}

	if (!PackUtils::UnpackChunk(begin, end, m_message.m_data, dataSize)) {
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

void CApplicationBuilder::UnpackExtensions(
	Message::Buffer::const_iterator& begin,
	Message::Buffer::const_iterator const& end)
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

Command::Type local::UnpackCommand(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end)
{
    using CommandType = std::underlying_type_t<Command::Type>;
    CommandType command = 0;
    PackUtils::UnpackChunk(begin, end, command);

    switch (command) {
        case static_cast<CommandType>(Command::Type::Connect):
        case static_cast<CommandType>(Command::Type::Election):
        case static_cast<CommandType>(Command::Type::Information):
        case static_cast<CommandType>(Command::Type::Query): {
            return static_cast<Command::Type>(command);
        }
        default: return Command::Type::Invalid;
    }
}

//------------------------------------------------------------------------------------------------

std::optional<Message::BoundTrackerKey> local::UnpackAwaitTracker(
    Message::Buffer::const_iterator& begin,
    Message::Buffer::const_iterator const& end,
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
