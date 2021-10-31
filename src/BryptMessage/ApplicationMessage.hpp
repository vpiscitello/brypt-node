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
#include "Components/Handler/HandlerDefinitions.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <optional>
#include <span>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Message {
//----------------------------------------------------------------------------------------------------------------------

enum class AwaitBinding : std::uint8_t { Invalid, Source, Destination };

using BoundTrackerKey = std::pair<AwaitBinding, Await::TrackerKey>;

//----------------------------------------------------------------------------------------------------------------------
} // Message namespace
//----------------------------------------------------------------------------------------------------------------------

class ApplicationBuilder;

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
	Message::Buffer GetPayload() const;
	std::optional<Await::TrackerKey> GetAwaitTrackerKey() const;

    std::size_t GetPackSize() const;
	std::string GetPack() const;
	Message::ShareablePack GetShareablePack() const;
	Message::ValidationStatus Validate() const;

private:
	constexpr std::size_t FixedPackSize() const;
	constexpr std::size_t FixedAwaitExtensionSize() const;

	MessageContext m_context; // The internal message context of the message
	MessageHeader m_header; // The required message header 

	std::string m_route; // The application route
	Message::Buffer m_payload;	// The message payload

	// Optional Extension: Allows the sender to associate to the destination's response with a 
	// a hopped or flooded request from another peer. 
	std::optional<Message::BoundTrackerKey> m_optBoundAwaitTracker;
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
	ApplicationBuilder& BindAwaitTracker(Message::AwaitBinding binding, Await::TrackerKey key);
	ApplicationBuilder& BindAwaitTracker(
		std::optional<Message::BoundTrackerKey> const& optBoundAwaitTracker);

	ApplicationBuilder& FromDecodedPack(std::span<std::uint8_t const> buffer);
	ApplicationBuilder& FromEncodedPack(std::string_view pack);

    ApplicationMessage&& Build();
    OptionalMessage ValidatedBuild();

private:
	void Unpack(std::span<std::uint8_t const> buffer);
	void UnpackExtensions(Message::Buffer::const_iterator& begin, Message::Buffer::const_iterator const& end);

    ApplicationMessage m_message;
	bool m_hasStageFailure;
};

//----------------------------------------------------------------------------------------------------------------------
