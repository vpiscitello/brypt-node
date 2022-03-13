//----------------------------------------------------------------------------------------------------------------------
// File: ApplicationMessage.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "ApplicationMessage.hpp"
#include "PackUtils.hpp"
#include "Utilities/Z85.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

Message::Application::Parcel::Parcel()
	: m_context()
	, m_header()
	, m_route()
	, m_payload()
	, m_extensions()
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Parcel::Parcel(Parcel const& other)
	: m_context(other.m_context)
	, m_header(other.m_header)
	, m_route(other.m_route)
	, m_payload(other.m_payload)
	, m_extensions()
{
	std::ranges::for_each(other.m_extensions, [&] (auto const& entry) {
		m_extensions.emplace(entry.second->GetKey(), entry.second->Clone());
	});
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Parcel& Message::Application::Parcel::operator=(Parcel const& other)
{
	m_context = other.m_context;
	m_header = other.m_header;
	m_route = other.m_route;
	m_payload = other.m_payload;
	std::ranges::for_each(other.m_extensions, [&] (auto const& entry) {
		m_extensions.emplace(entry.second->GetKey(), entry.second->Clone());
	});

	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder Message::Application::Parcel::GetBuilder() { return Builder{}; }

//----------------------------------------------------------------------------------------------------------------------

Message::Context const& Message::Application::Parcel::GetContext() const { return m_context; }

//----------------------------------------------------------------------------------------------------------------------

Message::Header const& Message::Application::Parcel::GetHeader() const { return m_header; }

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& Message::Application::Parcel::GetSource() const { return m_header.GetSource(); }

//----------------------------------------------------------------------------------------------------------------------

Message::Destination Message::Application::Parcel::GetDestinationType() const { return m_header.GetDestinationType(); }

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> const& Message::Application::Parcel::GetDestination() const
{
	return m_header.GetDestination();
}

//----------------------------------------------------------------------------------------------------------------------

std::string const& Message::Application::Parcel::GetRoute() const { return m_route; }

//----------------------------------------------------------------------------------------------------------------------

Message::Buffer const& Message::Application::Parcel::GetPayload() const { return m_payload; }

//----------------------------------------------------------------------------------------------------------------------

Message::Buffer&& Message::Application::Parcel::ExtractPayload() { return std::move(m_payload); }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Application::Parcel::GetPackSize() const
{
	std::size_t size = FixedPackSize();
	size += m_header.GetPackSize();
	size += m_route.size();
	size += m_payload.size();
	std::ranges::for_each(m_extensions, [&size] (auto const& entry) { size += entry.second->GetPackSize(); });

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
std::string Message::Application::Parcel::GetPack() const
{
	assert(m_context.HasSecurityHandlers()); 

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
		assert(std::in_range<std::uint8_t>(m_extensions.size()));
		PackUtils::PackChunk(static_cast<std::uint8_t>(m_extensions.size()), plaintext);
		std::ranges::for_each(m_extensions, [&plaintext] (auto const& entry) {
			entry.second->Inject(plaintext);
		});

		auto optEncryptedBuffer = m_context.Encrypt(plaintext, m_header.GetTimestamp());
		if (!optEncryptedBuffer) { return {}; }

		std::ranges::move(*optEncryptedBuffer, std::back_inserter(buffer));
	}
	
	// Calculate the number of bytes needed to pad to next 4 byte boundary
	std::size_t const pad = (4 - (buffer.size() & 3)) & 3;
	// Pad the message to ensure the encoding method doesn't add padding to the end of the message.
	Message::Buffer padding(pad, 0);
	buffer.insert(buffer.end(), padding.begin(), padding.end());

	if (m_context.Sign(buffer) < 0) { return {}; }

	return Z85::Encode(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

Message::ShareablePack Message::Application::Parcel::GetShareablePack() const
{
	return std::make_shared<std::string const>(GetPack());
}

//----------------------------------------------------------------------------------------------------------------------

Message::ValidationStatus Message::Application::Parcel::Validate() const
{
	using enum Message::ValidationStatus;

	if (!m_header.IsValid()) { return Error; } // A message must have a valid header
	if (m_route.empty()) { return Error; } // A message must identify a valid route

	bool const valid = std::ranges::all_of(m_extensions, [] (auto const& entry) { return entry.second->Validate(); });
	if (!valid) { return Error; }

	return Success;
}

//----------------------------------------------------------------------------------------------------------------------

constexpr std::size_t Message::Application::Parcel::FixedPackSize() const
{
	std::size_t size = 0;
	size += sizeof(std::uint16_t); // 2 byte for route size
	size += sizeof(std::uint32_t); // 4 bytes for payload size
	size += sizeof(std::uint8_t); // 1 byte for extensions size
    assert(std::in_range<std::uint32_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder::Builder()
    : m_parcel()
	, m_hasStageFailure(false)
{
	m_parcel.m_header.m_protocol = Message::Protocol::Application;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetContext(Context const& context)
{
	m_parcel.m_context = context;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetSource(Node::Identifier const& identifier)
{
	m_parcel.m_header.m_source = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Node::Identifier const& Message::Application::Builder::GetSource() const
{
	return m_parcel.m_header.m_source;
}

//----------------------------------------------------------------------------------------------------------------------

std::optional<Node::Identifier> const& Message::Application::Builder::GetDestination() const
{
	return m_parcel.m_header.m_optDestinationIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetSource(
    Node::Internal::Identifier const& identifier)
{
	m_parcel.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetSource(std::string_view identifier)
{
	m_parcel.m_header.m_source = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::MakeClusterMessage()
{
	m_parcel.m_header.m_destination = Message::Destination::Cluster;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------


Message::Application::Builder& Message::Application::Builder::MakeNetworkMessage()
{
	m_parcel.m_header.m_destination = Message::Destination::Network;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetDestination(Node::Identifier const& identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = identifier;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetDestination(
    Node::Internal::Identifier const& identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetDestination(std::string_view identifier)
{
	m_parcel.m_header.m_optDestinationIdentifier = Node::Identifier(identifier);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetRoute(std::string_view route)
{
	m_parcel.m_route = route;
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetRoute(std::string&& route)
{
	m_parcel.m_route = std::move(route);
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetPayload(std::string_view buffer)
{
    return SetPayload({ reinterpret_cast<std::uint8_t const*>(buffer.begin()), buffer.size() });
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetPayload(std::span<std::uint8_t const> buffer)
{
    return SetPayload(Message::Buffer{ buffer.begin(), buffer.end() });
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::SetPayload(Message::Buffer&& buffer)
{
	m_parcel.m_payload = std::move(buffer);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::FromDecodedPack(std::span<std::uint8_t const> buffer)
{
	assert(m_parcel.m_context.HasSecurityHandlers());

    if (buffer.empty()) { return *this; }

	bool const verified = m_parcel.m_context.Verify(buffer) == Security::VerificationStatus::Success;
	bool const success = verified && Unpack(buffer);
	if (!success) { m_hasStageFailure = true; }

    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder& Message::Application::Builder::FromEncodedPack(std::string_view pack)
{
	assert(m_parcel.m_context.HasSecurityHandlers());

    if (pack.empty()) { m_hasStageFailure = true; return *this; }
    
	auto const buffer = Z85::Decode(pack);
	bool const verified = m_parcel.m_context.Verify(buffer) == Security::VerificationStatus::Success;
	bool const success = verified && Unpack(buffer);
	if (!success) { m_hasStageFailure = true; }

    return *this;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Parcel&& Message::Application::Builder::Build()
{
	m_parcel.m_header.m_size = static_cast<std::uint32_t>(m_parcel.GetPackSize()); // Set the estimated message size.
    return std::move(m_parcel);
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Builder::OptionalParcel Message::Application::Builder::ValidatedBuild()
{
	m_parcel.m_header.m_size = static_cast<std::uint32_t>(m_parcel.GetPackSize()); // Set the estimated message size.
    if (m_hasStageFailure || m_parcel.Validate() != Message::ValidationStatus::Success) { return {}; }
    return std::move(m_parcel);
}

//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: Unpack the raw message string into the Message class variables.
//----------------------------------------------------------------------------------------------------------------------
bool Message::Application::Builder::Unpack(std::span<std::uint8_t const> buffer)
{
	assert(m_parcel.m_context.HasSecurityHandlers()); 

	{
		auto begin = buffer.begin();
		auto end = buffer.end();
		if (!m_parcel.m_header.ParseBuffer(begin, end)) { return false; }
	}

	// If the message in the buffer is not an application message, it can not be parsed
	if (m_parcel.m_header.m_protocol != Message::Protocol::Application) { return false; }

	// Create a view of the encrypted portion of the application message. This will be from the end of the header to 
	// the beginning of the signature. 
	Security::ReadableView bufferview = { 
		buffer.begin() + m_parcel.m_header.GetPackSize(),
		buffer.size() - m_parcel.m_header.GetPackSize() - m_parcel.m_context.GetSignatureSize() };

	auto const optDecryptedBuffer = m_parcel.m_context.Decrypt(bufferview, m_parcel.m_header.GetTimestamp());
	if (!optDecryptedBuffer) { return false; }

	auto begin = optDecryptedBuffer->begin();
	auto end = optDecryptedBuffer->end();
	{
		std::uint16_t size = 0;
		if (!PackUtils::UnpackChunk(begin, end, size)) { return false; }
		if (!PackUtils::UnpackChunk(begin, end, m_parcel.m_route, size)) { return false; }
	}
 
	{
		std::uint32_t size = 0;
		if (!PackUtils::UnpackChunk(begin, end, size)) { return false; }
		if (!PackUtils::UnpackChunk(begin, end, m_parcel.m_payload, size)) { return false; }
	}

	std::uint8_t extensions = 0;
	if (!PackUtils::UnpackChunk(begin, end, extensions)) { return false; }
	if (extensions != 0 && !UnpackExtensions(begin, end, extensions)) { return false; }

	return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Application::Builder::UnpackExtensions(
	Message::Buffer::const_iterator& begin, Message::Buffer::const_iterator const& end, std::size_t extensions)
{
	std::size_t unpacked = 0;	
	while (begin != end && unpacked < extensions) {
		Extension::Key key = 0;
		PackUtils::UnpackChunk(begin, end, key);

		switch (key){
			case Extension::Awaitable::Key: {
				auto upExtension = std::make_unique<Extension::Awaitable>();
				if (!upExtension->Unpack(begin, end)) { return false; }
				m_parcel.m_extensions.emplace(Extension::Awaitable::Key, std::move(upExtension));
			} break;					
			default: return false;
		}

		++unpacked;
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Extension::Awaitable::Awaitable()
	: m_binding(Invalid)
	, m_tracker()
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Extension::Awaitable::Awaitable(Binding binding, ::Awaitable::TrackerKey tracker)
	: m_binding(binding)
	, m_tracker(tracker)
{
}

//----------------------------------------------------------------------------------------------------------------------

std::uint16_t Message::Application::Extension::Awaitable::GetKey() const { return Key; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Application::Extension::Awaitable::GetPackSize() const 
{
	std::size_t size = 0;
	size += sizeof(Key); // 1 byte for the extension type
	size += sizeof(std::uint16_t); // 2 bytes for the extension size
	size += sizeof(m_binding); // 1 byte for await tracker binding
	size += sizeof(m_tracker); // 16 bytes for await tracker key
	assert(std::in_range<std::uint16_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Message::Application::Extension::Base> Message::Application::Extension::Awaitable::Clone() const
{
	return std::make_unique<Awaitable>(m_binding, m_tracker);
}

//----------------------------------------------------------------------------------------------------------------------

void Message::Application::Extension::Awaitable::Inject(Buffer& buffer) const
{
	std::uint16_t const size = static_cast<std::uint16_t>(GetPackSize());
	PackUtils::PackChunk(Key, buffer);
	PackUtils::PackChunk(size, buffer);
	PackUtils::PackChunk(m_binding, buffer);
	PackUtils::PackChunk(m_tracker, buffer);
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Application::Extension::Awaitable::Unpack(Buffer::const_iterator& begin, Buffer::const_iterator const& end)
{
	std::uint16_t size = 0;
	if (!PackUtils::UnpackChunk(begin, end, size)) { return false; }
	if (std::cmp_less(size, GetPackSize())) { return false; }

	{
		using BindingType = std::underlying_type_t<Binding>;
		BindingType binding = 0;
		if (!PackUtils::UnpackChunk(begin, end, binding)) { return {}; }
		switch (binding) {
			case static_cast<BindingType>(Request): { m_binding = Request; } break;
			case static_cast<BindingType>(Response):  { m_binding = Response; } break;
			default: return false;
		}
	}

	if (!PackUtils::UnpackChunk(begin, end, m_tracker)) { return false; }

	return true;
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Application::Extension::Awaitable::Validate() const
{
	return m_binding != Invalid && std::ranges::all_of(m_tracker, [] (std::uint8_t byte) {
		return byte != 0;
	});
}

//----------------------------------------------------------------------------------------------------------------------

Message::Application::Extension::Awaitable::Binding const& Message::Application::Extension::Awaitable::GetBinding() const
{
	return m_binding;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::TrackerKey const& Message::Application::Extension::Awaitable::GetTracker() const { return m_tracker; }

//----------------------------------------------------------------------------------------------------------------------
