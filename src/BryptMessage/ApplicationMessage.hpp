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
#include "Components/Await/AwaitDefinitions.hpp"
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
namespace Message::Extension {
//----------------------------------------------------------------------------------------------------------------------

using Key = std::uint8_t;
class Base;
class Awaitable;

//----------------------------------------------------------------------------------------------------------------------
} // Message::Extension namespace
//----------------------------------------------------------------------------------------------------------------------

class ApplicationBuilder;

//----------------------------------------------------------------------------------------------------------------------

class Message::Extension::Base
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

class ApplicationMessage {
public:
	ApplicationMessage();
	ApplicationMessage(ApplicationMessage const& other);

	// ApplicationBuilder {
	friend class ApplicationBuilder;
	static ApplicationBuilder Builder();
	// } ApplicationBuilder

	MessageContext const& GetContext() const;

	MessageHeader const& GetMessageHeader() const;
	Node::Identifier const& GetSourceIdentifier() const;
	Message::Destination GetDestinationType() const;
	std::optional<Node::Identifier> const& GetDestinationIdentifier() const;

	std::string const& GetRoute() const;
	Message::Buffer const& GetPayload() const;

	template<typename ExtensionType> requires std::derived_from<ExtensionType, Message::Extension::Base>
	std::optional<std::reference_wrapper<ExtensionType const>> GetExtension() const;

    std::size_t GetPackSize() const;
	std::string GetPack() const;
	Message::ShareablePack GetShareablePack() const;
	Message::ValidationStatus Validate() const;

private:
	constexpr std::size_t FixedPackSize() const;

	MessageContext m_context; // The internal message context of the message
	MessageHeader m_header; // The required message header 

	std::string m_route; // The application route
	Message::Buffer m_payload;	// The message payload

	std::map<Message::Extension::Key, std::unique_ptr<Message::Extension::Base>> m_extensions;
};

//----------------------------------------------------------------------------------------------------------------------

class ApplicationBuilder {
public:
	using OptionalMessage = std::optional<ApplicationMessage>;

	ApplicationBuilder();

	ApplicationBuilder& SetMessageContext(MessageContext const& context);
	ApplicationBuilder& SetSource(Node::Identifier const& identifier);
	ApplicationBuilder& SetSource(Node::Internal::Identifier const& identifier);
	ApplicationBuilder& SetSource(std::string_view identifier);
	ApplicationBuilder& MakeClusterMessage();
	ApplicationBuilder& MakeNetworkMessage();
	ApplicationBuilder& SetDestination(Node::Identifier const& identifier);
	ApplicationBuilder& SetDestination(Node::Internal::Identifier const& identifier);
	ApplicationBuilder& SetDestination(std::string_view identifier);
	ApplicationBuilder& SetRoute(std::string_view const& route);
	ApplicationBuilder& SetRoute(std::string&& route);
	ApplicationBuilder& SetPayload(std::string_view buffer);
	ApplicationBuilder& SetPayload(std::span<std::uint8_t const> buffer);
	ApplicationBuilder& SetPayload(Message::Buffer&& buffer);

	template<typename ExtensionType, typename... Arguments>
		requires std::derived_from<ExtensionType, Message::Extension::Base>
	ApplicationBuilder& BindExtension(Arguments&&... arguments);

	template<typename ExtensionType> requires std::derived_from<ExtensionType, Message::Extension::Base>
	ApplicationBuilder& BindExtension(std::unique_ptr<ExtensionType>&& upExtension);

	ApplicationBuilder& FromDecodedPack(std::span<std::uint8_t const> buffer);
	ApplicationBuilder& FromEncodedPack(std::string_view pack);

    ApplicationMessage&& Build();
    OptionalMessage ValidatedBuild();

private:
	[[nodiscard]] bool Unpack(std::span<std::uint8_t const> buffer);
	[[nodiscard]] bool UnpackExtensions(
		Message::Buffer::const_iterator& begin, Message::Buffer::const_iterator const& end);

    ApplicationMessage m_message;
	bool m_hasStageFailure;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Extension::Awaitable : public Message::Extension::Base
{
public:
	static constexpr Message::Extension::Key Key = 0xae;
	
	enum Binding : std::uint8_t { Invalid, Request, Response };
	
	Awaitable();
	Awaitable(Binding binding, Await::TrackerKey tracker);

	// Extension {
	[[nodiscard]] virtual std::uint16_t GetKey() const override;
	[[nodiscard]] virtual std::size_t GetPackSize() const override;
	[[nodiscard]] virtual std::unique_ptr<Base> Clone() const override;
	
	virtual void Inject(Buffer& buffer) const override;
	[[nodiscard]] virtual bool Unpack(Buffer::const_iterator& begin, Buffer::const_iterator const& end) override;
	[[nodiscard]] virtual bool Validate() const override;
	// } Extension

	[[nodiscard]] Binding const& GetBinding() const;
	[[nodiscard]] Await::TrackerKey const& GetTracker() const;

private:
	Binding m_binding;
	Await::TrackerKey m_tracker;
};

//----------------------------------------------------------------------------------------------------------------------

template<typename ExtensionType> requires std::derived_from<ExtensionType, Message::Extension::Base>
std::optional<std::reference_wrapper<ExtensionType const>> ApplicationMessage::GetExtension() const
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
	requires std::derived_from<ExtensionType, Message::Extension::Base>
ApplicationBuilder& ApplicationBuilder::BindExtension(Arguments&&... arguments)
{
	m_message.m_extensions.emplace(
		ExtensionType::Key, std::make_unique<ExtensionType>(std::forward<Arguments>(arguments)...));
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------

template<typename ExtensionType> requires std::derived_from<ExtensionType, Message::Extension::Base>
ApplicationBuilder& ApplicationBuilder::BindExtension(std::unique_ptr<ExtensionType>&& upExtension)
{
	m_message.m_extensions.emplace(ExtensionType::Key, std::move(upExtension));
	return *this;
}

//----------------------------------------------------------------------------------------------------------------------
