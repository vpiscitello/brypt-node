//----------------------------------------------------------------------------------------------------------------------
// File: ApplicationMessage.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "MessageHeader.hpp"
#include "MessageTypes.hpp"
#include "ShareablePack.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Awaitable/Definitions.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <concepts>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Message::Application {
//----------------------------------------------------------------------------------------------------------------------

class Parcel;
class Builder;

//----------------------------------------------------------------------------------------------------------------------
namespace Extension {
//----------------------------------------------------------------------------------------------------------------------

using Key = std::uint8_t;
class Base;
class Awaitable;

//----------------------------------------------------------------------------------------------------------------------
} // Extension namespace
//----------------------------------------------------------------------------------------------------------------------
} // Message::Application namespace
//----------------------------------------------------------------------------------------------------------------------

class Message::Application::Extension::Base
{
public:
	Base() = default;
	virtual ~Base() = default;
	[[nodiscard]] virtual std::uint16_t GetKey() const = 0;
	[[nodiscard]] virtual std::size_t GetPackSize() const = 0;
	[[nodiscard]] virtual std::unique_ptr<Base> Clone() const = 0;
	
	virtual void Inject(Buffer& buffer) const = 0;
	[[nodiscard]] virtual bool Unpack(Buffer::const_iterator& begin, Buffer::const_iterator const& end) = 0;
	[[nodiscard]] virtual bool Validate() const = 0;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Application::Parcel {
public:
	Parcel();
	Parcel(Parcel const& other);

	// Message::Application::Builder {
	friend class Builder;
	static Builder GetBuilder();
	// } Message::Application::Builder

	Context const& GetContext() const;
	Header const& GetHeader() const;
	Node::Identifier const& GetSourceIdentifier() const;
	Destination GetDestinationType() const;
	std::optional<Node::Identifier> const& GetDestinationIdentifier() const;

	std::string const& GetRoute() const;
	Message::Buffer const& GetPayload() const;

	template<typename ExtensionType> requires std::derived_from<ExtensionType, Extension::Base>
	std::optional<std::reference_wrapper<ExtensionType const>> GetExtension() const;

    std::size_t GetPackSize() const;
	std::string GetPack() const;
	ShareablePack GetShareablePack() const;
	ValidationStatus Validate() const;

private:
	constexpr std::size_t FixedPackSize() const;

	Context m_context; // The internal message context of the message
	Header m_header; // The required message header 

	std::string m_route; // The application route
	Buffer m_payload;	// The message payload

	std::map<Extension::Key, std::unique_ptr<Extension::Base>> m_extensions;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Application::Builder {
public:
	using OptionalParcel = std::optional<Parcel>;

	Builder();

	Builder& SetContext(Context const& context);
	Builder& SetSource(Node::Identifier const& identifier);
	Builder& SetSource(Node::Internal::Identifier const& identifier);
	Builder& SetSource(std::string_view identifier);
	Builder& MakeClusterMessage();
	Builder& MakeNetworkMessage();
	Builder& SetDestination(Node::Identifier const& identifier);
	Builder& SetDestination(Node::Internal::Identifier const& identifier);
	Builder& SetDestination(std::string_view identifier);
	Builder& SetRoute(std::string_view route);
	Builder& SetRoute(std::string&& route);
	Builder& SetPayload(std::string_view buffer);
	Builder& SetPayload(std::span<std::uint8_t const> buffer);
	Builder& SetPayload(Message::Buffer&& buffer);

	template<typename ExtensionType, typename... Arguments>
		requires std::derived_from<ExtensionType, Extension::Base>
	Builder& BindExtension(Arguments&&... arguments);

	template<typename ExtensionType> requires std::derived_from<ExtensionType, Extension::Base>
	Builder& BindExtension(std::unique_ptr<ExtensionType>&& upExtension);

	Builder& FromDecodedPack(std::span<std::uint8_t const> buffer);
	Builder& FromEncodedPack(std::string_view pack);

    Parcel&& Build();
    OptionalParcel ValidatedBuild();

private:
	[[nodiscard]] bool Unpack(std::span<std::uint8_t const> buffer);
	[[nodiscard]] bool UnpackExtensions(
		Message::Buffer::const_iterator& begin, Message::Buffer::const_iterator const& end);

    Parcel m_parcel;
	bool m_hasStageFailure;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Application::Extension::Awaitable : public Message::Application::Extension::Base
{
public:
	static constexpr Extension::Key Key = 0xae;
	
	enum Binding : std::uint8_t { Invalid, Request, Response };
	
	Awaitable();
	Awaitable(Binding binding, ::Awaitable::TrackerKey tracker);

	// Extension {
	[[nodiscard]] virtual std::uint16_t GetKey() const override;
	[[nodiscard]] virtual std::size_t GetPackSize() const override;
	[[nodiscard]] virtual std::unique_ptr<Base> Clone() const override;
	
	virtual void Inject(Buffer& buffer) const override;
	[[nodiscard]] virtual bool Unpack(Buffer::const_iterator& begin, Buffer::const_iterator const& end) override;
	[[nodiscard]] virtual bool Validate() const override;
	// } Extension

	[[nodiscard]] Binding const& GetBinding() const;
	[[nodiscard]] ::Awaitable::TrackerKey const& GetTracker() const;

private:
	Binding m_binding;
	::Awaitable::TrackerKey m_tracker;
};

//----------------------------------------------------------------------------------------------------------------------

template<typename ExtensionType> requires std::derived_from<ExtensionType, Message::Application::Extension::Base>
std::optional<std::reference_wrapper<ExtensionType const>> Message::Application::Parcel::GetExtension() const
{
	if (auto const itr = m_extensions.find(ExtensionType::Key); itr != m_extensions.end()) {
		auto const pExtension = dynamic_cast<ExtensionType const*>(itr->second.get());
		assert(pExtension);
		return *pExtension;
	}
	return {};
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ExtensionType, typename... Arguments>
	requires std::derived_from<ExtensionType, Message::Application::Extension::Base>
Message::Application::Builder& Message::Application::Builder::BindExtension(Arguments&&... arguments)
{
	m_parcel.m_extensions.emplace(
		ExtensionType::Key, std::make_unique<ExtensionType>(std::forward<Arguments>(arguments)...));
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ExtensionType> requires std::derived_from<ExtensionType, Message::Application::Extension::Base>
Message::Application::Builder& Message::Application::Builder::BindExtension(std::unique_ptr<ExtensionType>&& upExtension)
{
	m_parcel.m_extensions.emplace(ExtensionType::Key, std::move(upExtension));
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
