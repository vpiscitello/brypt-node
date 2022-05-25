//----------------------------------------------------------------------------------------------------------------------
// File: ApplicationMessage.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "DataInterface.hpp"
#include "Components/Awaitable/Definitions.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
#include <span>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Message::Extension {
//----------------------------------------------------------------------------------------------------------------------

using Key = std::uint16_t;
class Base;
class Awaitable;
class Status;

//----------------------------------------------------------------------------------------------------------------------
} // Message::Extension namespace
//----------------------------------------------------------------------------------------------------------------------

class Message::Extension::Base : public virtual Message::DataInterface::Packable
{
public:
	Base() = default;
	virtual ~Base() = default;

	// Packable {
	[[nodiscard]] virtual std::size_t GetPackSize() const override;
	virtual void Inject(Buffer& buffer) const = 0;
	[[nodiscard]] virtual bool Unpack(
		std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end) = 0;
	// } Packable

	[[nodiscard]] virtual Key GetKey() const = 0;
	[[nodiscard]] virtual std::unique_ptr<Base> Clone() const = 0;
	[[nodiscard]] virtual bool Validate() const = 0;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Extension::Awaitable : public Message::Extension::Base
{
public:
	static constexpr Extension::Key Key = 0xaabe;
	
	enum Binding : std::uint8_t { Invalid = 0x00, Request = 0x01, Response = 0x10 };
	
	Awaitable();
	Awaitable(Binding binding, ::Awaitable::TrackerKey tracker);
	
	[[nodiscard]] bool operator==(Awaitable const& other) const;

	// Extension {
	[[nodiscard]] virtual Extension::Key GetKey() const override;
	[[nodiscard]] virtual std::unique_ptr<Base> Clone() const override;
	[[nodiscard]] virtual bool Validate() const override;
	// } Extension
	
	// Packable {
	[[nodiscard]] virtual std::size_t GetPackSize() const override;
	virtual void Inject(Buffer& buffer) const override;
	[[nodiscard]] virtual bool Unpack(
		std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end) override;
	// } Packable

	[[nodiscard]] Binding const& GetBinding() const;
	[[nodiscard]] ::Awaitable::TrackerKey const& GetTracker() const;

private:
	Binding m_binding;
	::Awaitable::TrackerKey m_tracker;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Extension::Status : public Message::Extension::Base
{
public:
	static constexpr Extension::Key Key = 0xc0de;
	
	enum Code : std::uint16_t { 
		Unknown = 0,
		Ok = 200,
		Created = 201,
		Accepted = 202,
		NoContent = 204,
		PartialContent = 206,
		MovedPermanently = 301,
		Found = 302,
		NotModified = 304,
		TemporaryRedirect = 307,
		PermanentRedirect = 308,
		BadRequest = 400,
		Unauthorized = 401,
		Forbidden = 403,
		NotFound = 404,
		RequestTimeout = 408,
		Conflict = 409,
		PayloadTooLarge = 413,
		UriTooLong = 414,
		ImATeapot = 418,
		Locked = 423,
		UnavailableForLegalReasons = 451,
		UpgradeRequired = 426,
		TooManyRequests = 429,
		InternalServerError = 500,
		NotImplemented = 501,
		ServiceUnavailable = 503,
		InsufficientStorage = 507,
		LoopDetected = 508,
	};
	
	Status();
	Status(Code code);
	
	[[nodiscard]] bool operator==(Status const& other) const;

	// Extension {
	[[nodiscard]] virtual Extension::Key GetKey() const override;
	[[nodiscard]] virtual std::unique_ptr<Base> Clone() const override;
	[[nodiscard]] virtual bool Validate() const override;
	// } Extension
	
	// Packable {
	[[nodiscard]] virtual std::size_t GetPackSize() const override;
	virtual void Inject(Buffer& buffer) const override;
	[[nodiscard]] virtual bool Unpack(
		std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end) override;
	// } Packable

	[[nodiscard]] static constexpr bool IsSuccessCode(Code code);
	[[nodiscard]] static constexpr bool IsErrorCode(Code code);

	[[nodiscard]] bool HasSuccessCode() const;
	[[nodiscard]] bool HasErrorCode() const;

	[[nodiscard]] Code const& GetCode() const;

private:
	Code m_code;
};

//----------------------------------------------------------------------------------------------------------------------
