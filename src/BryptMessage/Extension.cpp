//----------------------------------------------------------------------------------------------------------------------
#include "Extension.hpp"
#include "PackUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <algorithm>
#include <ranges>
//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Extension::Base::GetPackSize() const 
{
	std::size_t size = 0;
	size += sizeof(Key); // 1 byte for the extension type
	size += sizeof(std::uint16_t); // 2 bytes for the extension size
	assert(std::in_range<std::uint16_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Awaitable::Awaitable()
	: m_binding(Invalid)
	, m_tracker()
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Awaitable::Awaitable(Binding binding, ::Awaitable::TrackerKey tracker)
	: m_binding(binding)
	, m_tracker(tracker)
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Extension::Awaitable::operator==(Awaitable const& other) const
{
	return m_binding == other.m_binding && m_tracker == m_tracker;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Key Message::Extension::Awaitable::GetKey() const { return Key; }

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Message::Extension::Base> Message::Extension::Awaitable::Clone() const
{
	return std::make_unique<Awaitable>(m_binding, m_tracker);
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Extension::Awaitable::Validate() const
{
	return m_binding != Invalid && !std::ranges::all_of(m_tracker, [] (std::uint8_t byte) { return byte == 0x00; });
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Extension::Awaitable::GetPackSize() const 
{
	std::size_t size = Base::GetPackSize();
	size += sizeof(m_binding); // 1 byte for await tracker binding
	size += sizeof(m_tracker); // 16 bytes for await tracker key
	assert(std::in_range<std::uint16_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

void Message::Extension::Awaitable::Inject(Buffer& buffer) const
{
	std::uint16_t const size = static_cast<std::uint16_t>(GetPackSize());
	PackUtils::PackChunk(Key, buffer);
	PackUtils::PackChunk(size, buffer);
	PackUtils::PackChunk(m_binding, buffer);
	PackUtils::PackChunk(m_tracker, buffer);
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Extension::Awaitable::Unpack(
	std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end)
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

Message::Extension::Awaitable::Binding const& Message::Extension::Awaitable::GetBinding() const
{
	return m_binding;
}

//----------------------------------------------------------------------------------------------------------------------

Awaitable::TrackerKey const& Message::Extension::Awaitable::GetTracker() const { return m_tracker; }

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Status::Status()
	: m_code(Unknown)
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Status::Status(Code code)
	: m_code(code)
{
}

//----------------------------------------------------------------------------------------------------------------------

constexpr bool Message::Extension::Status::IsSuccessCode(Code code)
{
	return static_cast<std::underlying_type_t<Code>>(code) < 300; // Codes 0 - 299 are indicative of a success.
}

//----------------------------------------------------------------------------------------------------------------------

constexpr bool Message::Extension::Status::IsErrorCode(Code code) { return !IsSuccessCode(code); }

//----------------------------------------------------------------------------------------------------------------------

bool Message::Extension::Status::HasSuccessCode() const { return IsSuccessCode(m_code); }

//----------------------------------------------------------------------------------------------------------------------

bool Message::Extension::Status::HasErrorCode() const { return IsErrorCode(m_code); }

//----------------------------------------------------------------------------------------------------------------------

bool Message::Extension::Status::operator==(Status const& other) const { return m_code == other.m_code; }

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Key Message::Extension::Status::GetKey() const { return Key; }

//----------------------------------------------------------------------------------------------------------------------

std::unique_ptr<Message::Extension::Base> Message::Extension::Status::Clone() const
{
	return std::make_unique<Status>(m_code);
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Extension::Status::Validate() const { return m_code != Unknown; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Extension::Status::GetPackSize() const 
{
	std::size_t size = Base::GetPackSize();
	size += sizeof(m_code); // 16 bytes for await tracker key
	assert(std::in_range<std::uint16_t>(size));
	return size;
}

//----------------------------------------------------------------------------------------------------------------------

void Message::Extension::Status::Inject(Buffer& buffer) const
{
	std::uint16_t const size = static_cast<std::uint16_t>(GetPackSize());
	PackUtils::PackChunk(Key, buffer);
	PackUtils::PackChunk(size, buffer);
	PackUtils::PackChunk(m_code, buffer);
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Extension::Status::Unpack(
	std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end)
{
	std::uint16_t size = 0;
	if (!PackUtils::UnpackChunk(begin, end, size)) { return false; }
	if (std::cmp_less(size, GetPackSize())) { return false; }

	{
		using CodeType = std::underlying_type_t<Code>;
		CodeType code = 0;
		if (!PackUtils::UnpackChunk(begin, end, code)) { return {}; }
		switch (code) {
			case static_cast<CodeType>(Ok): { m_code = Ok; } break;
			case static_cast<CodeType>(Created): { m_code = Created; } break;
			case static_cast<CodeType>(Accepted): { m_code = Accepted; } break;
			case static_cast<CodeType>(NoContent): { m_code = NoContent; } break;
			case static_cast<CodeType>(PartialContent): { m_code = PartialContent; } break;
			case static_cast<CodeType>(MovedPermanently): { m_code = MovedPermanently; } break;
			case static_cast<CodeType>(Found): { m_code = Found; } break;
			case static_cast<CodeType>(NotModified): { m_code = NotModified; } break;
			case static_cast<CodeType>(TemporaryRedirect): { m_code = TemporaryRedirect; } break;
			case static_cast<CodeType>(PermanentRedirect): { m_code = PermanentRedirect; } break;
			case static_cast<CodeType>(BadRequest): { m_code = BadRequest; } break;
			case static_cast<CodeType>(Unauthorized): { m_code = Unauthorized; } break;
			case static_cast<CodeType>(Forbidden): { m_code = Forbidden; } break;
			case static_cast<CodeType>(NotFound): { m_code = NotFound; } break;
			case static_cast<CodeType>(RequestTimeout): { m_code = RequestTimeout; } break;
			case static_cast<CodeType>(Conflict): { m_code = Conflict; } break;
			case static_cast<CodeType>(PayloadTooLarge): { m_code = PayloadTooLarge; } break;
			case static_cast<CodeType>(UriTooLong): { m_code = UriTooLong; } break;
			case static_cast<CodeType>(ImATeapot): { m_code = ImATeapot; } break;
			case static_cast<CodeType>(Locked): { m_code = Locked; } break;
			case static_cast<CodeType>(UpgradeRequired): { m_code = UpgradeRequired; } break;
			case static_cast<CodeType>(TooManyRequests): { m_code = TooManyRequests; } break;
			case static_cast<CodeType>(UnavailableForLegalReasons): { m_code = UnavailableForLegalReasons; } break;
			case static_cast<CodeType>(InternalServerError): { m_code = InternalServerError; } break;
			case static_cast<CodeType>(NotImplemented): { m_code = NotImplemented; } break;
			case static_cast<CodeType>(ServiceUnavailable): { m_code = ServiceUnavailable; } break;
			case static_cast<CodeType>(InsufficientStorage): { m_code = InsufficientStorage; } break;
			case static_cast<CodeType>(LoopDetected): { m_code = LoopDetected; } break;
			default: return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------------------------------

Message::Extension::Status::Code const& Message::Extension::Status::GetCode() const
{
	return m_code;
}

//----------------------------------------------------------------------------------------------------------------------
