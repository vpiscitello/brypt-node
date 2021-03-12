//----------------------------------------------------------------------------------------------------------------------
// File: ApplicationMessage.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "MessageHeader.hpp"
#include "MessageTypes.hpp"
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
	BryptIdentifier::Container const& GetSourceIdentifier() const;
	Message::Destination GetDestinationType() const;
	std::optional<BryptIdentifier::Container> const& GetDestinationIdentifier() const;

	Handler::Type GetCommand() const;
	std::uint8_t GetPhase() const;
	Message::Buffer GetPayload() const;
	TimeUtils::Timestamp const& GetTimestamp() const;
	std::optional<Await::TrackerKey> GetAwaitTrackerKey() const;

    std::size_t GetPackSize() const;
	std::string GetPack() const;
	Message::ValidationStatus Validate() const;

private:
	constexpr std::size_t FixedPackSize() const;
	constexpr std::size_t FixedAwaitExtensionSize() const;

	MessageContext m_context; // The internal message context of the message
	MessageHeader m_header; // The required message header 

	Handler::Type m_command; // The application command
	std::uint8_t m_phase; // The command phase
	Message::Buffer m_payload;	// The command payload
	TimeUtils::Timestamp m_timestamp; // The message creation timestamp

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
	ApplicationBuilder& SetSource(BryptIdentifier::Container const& identifier);
	ApplicationBuilder& SetSource(BryptIdentifier::Internal::Type const& identifier);
	ApplicationBuilder& SetSource(std::string_view identifier);
	ApplicationBuilder& MakeClusterMessage();
	ApplicationBuilder& MakeNetworkMessage();
	ApplicationBuilder& SetDestination(BryptIdentifier::Container const& identifier);
	ApplicationBuilder& SetDestination(BryptIdentifier::Internal::Type const& identifier);
	ApplicationBuilder& SetDestination(std::string_view identifier);
	ApplicationBuilder& SetCommand(Handler::Type type, std::uint8_t phase);
	ApplicationBuilder& SetPayload(std::string_view buffer);
	ApplicationBuilder& SetPayload(std::span<std::uint8_t const> buffer);
	ApplicationBuilder& BindAwaitTracker(Message::AwaitBinding binding, Await::TrackerKey key);
	ApplicationBuilder& BindAwaitTracker(
		std::optional<Message::BoundTrackerKey> const& optBoundAwaitTracker);

	ApplicationBuilder& FromDecodedPack(std::span<std::uint8_t const> buffer);
	ApplicationBuilder& FromEncodedPack(std::string_view pack);

    ApplicationMessage&& Build();
    OptionalMessage ValidatedBuild();

private:
	void Unpack(std::span<std::uint8_t const> buffer);
	void UnpackExtensions(
        std::span<std::uint8_t const>::iterator& begin,
        std::span<std::uint8_t const>::iterator const& end);

    ApplicationMessage m_message;
};

//----------------------------------------------------------------------------------------------------------------------
