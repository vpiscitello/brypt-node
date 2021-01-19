//------------------------------------------------------------------------------------------------
// File: ApplicationMessage.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "MessageHeader.hpp"
#include "MessageTypes.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Components/Await/AwaitDefinitions.hpp"
#include "Components/Handler/HandlerDefinitions.hpp"
#include "Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------
#include <optional>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Message {
//------------------------------------------------------------------------------------------------

enum class AwaitBinding : std::uint8_t { Invalid, Source, Destination };

using BoundTrackerKey = std::pair<AwaitBinding, Await::TrackerKey>;

//------------------------------------------------------------------------------------------------
} // Message namespace
//------------------------------------------------------------------------------------------------

class CApplicationBuilder;

//------------------------------------------------------------------------------------------------

class CApplicationMessage {
public:
	CApplicationMessage();
	CApplicationMessage(CApplicationMessage const& other);

	// CApplicationBuilder {
	friend class CApplicationBuilder;
	static CApplicationBuilder Builder();
	// } CApplicationBuilder

	CMessageContext const& GetContext() const;

	CMessageHeader const& GetMessageHeader() const;
	BryptIdentifier::CContainer const& GetSourceIdentifier() const;
	Message::Destination GetDestinationType() const;
	std::optional<BryptIdentifier::CContainer> const& GetDestinationIdentifier() const;

	Command::Type GetCommand() const;
	std::uint8_t GetPhase() const;
	Message::Buffer GetPayload() const;
	TimeUtils::Timestamp const& GetTimestamp() const;
	std::optional<Await::TrackerKey> GetAwaitTrackerKey() const;

    std::uint32_t GetPackSize() const;
	std::string GetPack() const;
	Message::ValidationStatus Validate() const;

private:
	constexpr std::uint32_t FixedPackSize() const;
	constexpr std::uint16_t FixedAwaitExtensionSize() const;

	CMessageContext m_context; // The internal message context of the message
	CMessageHeader m_header; // The required message header 

	Command::Type m_command; // The application command
	std::uint8_t m_phase; // The command phase
	Message::Buffer m_payload;	// The command payload
	TimeUtils::Timestamp m_timestamp; // The message creation timestamp

	// Optional Extension: Allows the sender to associate to the destination's response with a 
	// a hopped or flooded request from another peer. 
	std::optional<Message::BoundTrackerKey> m_optBoundAwaitTracker;

};

//------------------------------------------------------------------------------------------------

class CApplicationBuilder {
public:
	using OptionalMessage = std::optional<CApplicationMessage>;

	CApplicationBuilder();

	CApplicationBuilder& SetMessageContext(CMessageContext const& context);
	CApplicationBuilder& SetSource(BryptIdentifier::CContainer const& identifier);
	CApplicationBuilder& SetSource(BryptIdentifier::Internal::Type const& identifier);
	CApplicationBuilder& SetSource(std::string_view identifier);
	CApplicationBuilder& MakeClusterMessage();
	CApplicationBuilder& MakeNetworkMessage();
	CApplicationBuilder& SetDestination(BryptIdentifier::CContainer const& identifier);
	CApplicationBuilder& SetDestination(BryptIdentifier::Internal::Type const& identifier);
	CApplicationBuilder& SetDestination(std::string_view identifier);
	CApplicationBuilder& SetCommand(Command::Type type, std::uint8_t phase);
	CApplicationBuilder& SetPayload(std::string_view data);
	CApplicationBuilder& SetPayload(Message::Buffer const& buffer);
	CApplicationBuilder& BindAwaitTracker(Message::AwaitBinding binding, Await::TrackerKey key);
	CApplicationBuilder& BindAwaitTracker(
		std::optional<Message::BoundTrackerKey> const& optBoundAwaitTracker);

	CApplicationBuilder& FromDecodedPack(Message::Buffer const& buffer);
	CApplicationBuilder& FromEncodedPack(std::string_view pack);

    CApplicationMessage&& Build();
    OptionalMessage ValidatedBuild();

private:
	void Unpack(Message::Buffer const& buffer);
	void UnpackExtensions(
        Message::Buffer::const_iterator& begin,
        Message::Buffer::const_iterator const& end);

    CApplicationMessage m_message;

};

//------------------------------------------------------------------------------------------------
