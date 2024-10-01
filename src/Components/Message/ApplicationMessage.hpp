//----------------------------------------------------------------------------------------------------------------------
// File: ApplicationMessage.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "DataInterface.hpp"
#include "Extension.hpp"
#include "MessageContext.hpp"
#include "MessageHeader.hpp"
#include "MessageTypes.hpp"
#include "Payload.hpp"
#include "ShareablePack.hpp"
#include "Components/Identifier/BryptIdentifier.hpp"
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
} // Message::Application namespace
//----------------------------------------------------------------------------------------------------------------------

class Message::Application::Parcel
{
public:
	Parcel();
	Parcel(Parcel const& other);
    Parcel& operator=(Parcel const& other);
    
	Parcel(Parcel&&) = default;
    Parcel& operator=(Parcel&&) = default;

	[[nodiscard]] bool operator==(Parcel const& other) const;

	// Message::Application::Builder {
	friend class Builder;
	static Builder GetBuilder();
	// } Message::Application::Builder

	Context const& GetContext() const;
	Header const& GetHeader() const;
	Node::Identifier const& GetSource() const;
	Destination GetDestinationType() const;
	std::optional<Node::Identifier> const& GetDestination() const;

	std::string const& GetRoute() const;
	Payload const& GetPayload() const;
	Payload&& ExtractPayload();

	template<typename ExtensionType> requires std::derived_from<ExtensionType, Extension::Base>
	std::optional<std::reference_wrapper<ExtensionType const>> GetExtension() const
	{
		if (auto const itr = m_extensions.find(ExtensionType::Key); itr != m_extensions.end()) {
			auto const pExtension = dynamic_cast<ExtensionType const*>(itr->second.get());
			assert(pExtension);
			return *pExtension;
		}
		return {};
	}

    std::size_t GetPackSize() const;
	std::string GetPack() const;
	ShareablePack GetShareablePack() const;
	ValidationStatus Validate() const;

private:
	constexpr std::size_t FixedPackSize() const;

	Context m_context; // The internal message context of the message
	Header m_header; // The required message header 

	std::string m_route; // The application route
	Payload m_payload;	// The message payload

	std::map<Extension::Key, std::unique_ptr<Extension::Base>> m_extensions;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Application::Builder
{
public:
	using OptionalParcel = std::optional<Parcel>;

	Builder();

	Node::Identifier const& GetSource() const;
	std::optional<Node::Identifier> const& GetDestination() const;
	Context const& GetContext() const;

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
	Builder& SetPayload(Payload&& payload);
	Builder& SetPayload(Payload const& payload);

	template<typename ExtensionType, typename... Arguments>
		requires std::derived_from<ExtensionType, Extension::Base>
	Builder& BindExtension(Arguments&&... arguments)
	{
		m_parcel.m_extensions.emplace(
			ExtensionType::Key, std::make_unique<ExtensionType>(std::forward<Arguments>(arguments)...));
		return *this;
	}

	template<typename ExtensionType> requires std::derived_from<ExtensionType, Extension::Base>
	Builder& BindExtension(std::unique_ptr<ExtensionType>&& upExtension)
	{
		m_parcel.m_extensions.emplace(ExtensionType::Key, std::move(upExtension));
		return *this;
	}

	Builder& FromDecodedPack(std::span<std::uint8_t const> buffer);
	Builder& FromEncodedPack(std::string_view pack);

    Parcel&& Build();
    OptionalParcel ValidatedBuild();

private:
	[[nodiscard]] bool Unpack(std::span<std::uint8_t const> buffer);
	[[nodiscard]] bool UnpackExtensions(
		std::span<std::uint8_t const>::iterator& begin,
		std::span<std::uint8_t const>::iterator const& end,
		std::size_t extensions);

    Parcel m_parcel;
	bool m_hasStageFailure;
};

//----------------------------------------------------------------------------------------------------------------------
