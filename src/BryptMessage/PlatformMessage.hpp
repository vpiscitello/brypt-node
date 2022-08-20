//----------------------------------------------------------------------------------------------------------------------
// File: PlatformMessage.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "MessageHeader.hpp"
#include "MessageTypes.hpp"
#include "Payload.hpp"
#include "ShareablePack.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
#include "Utilities/InvokeContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <optional>
#include <span>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Message::Platform {
//----------------------------------------------------------------------------------------------------------------------

enum class ParcelType : std::uint8_t { Invalid, Handshake, HeartbeatRequest, HeartbeatResponse };

class Parcel;
class Builder;

//----------------------------------------------------------------------------------------------------------------------
} // Message::Platform namespace 
//----------------------------------------------------------------------------------------------------------------------

class Message::Platform::Parcel
{
public:
	Parcel();
	Parcel(Parcel const& other);
    Parcel& operator=(Parcel const& other);
    
	Parcel(Parcel&&) = default;
    Parcel& operator=(Parcel&&) = default;
	
	[[nodiscard]] bool operator==(Parcel const& other) const;

	// Platform::Builder {
	friend class Message::Platform::Builder;
	static Message::Platform::Builder GetBuilder();
	// } Platform::Builder

	Context const& GetContext() const;

	Header const& GetHeader() const;
	Node::Identifier const& GetSource() const;
	Message::Destination GetDestinationType() const;
	std::optional<Node::Identifier> const& GetDestination() const;
	ParcelType GetType() const;
	Payload const& GetPayload() const;
	Payload&& ExtractPayload();

	std::size_t GetPackSize() const;
	std::string GetPack() const;
	ShareablePack GetShareablePack() const;
	ValidationStatus Validate() const;

private:
	constexpr std::size_t FixedPackSize() const;

	Context m_context; // The internal message context of the message
	Header m_header; // The required message header 

	ParcelType m_type;
	Payload m_payload;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Platform::Builder
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
	Builder& SetDestination(Node::Identifier const& identifier);
	Builder& SetDestination(Node::Internal::Identifier const& identifier);
	Builder& SetDestination(std::string_view identifier);
	Builder& MakeHandshakeMessage();
	Builder& MakeHeartbeatRequest();
	Builder& MakeHeartbeatResponse();
	Builder& SetPayload(Payload&& payload);
	Builder& SetPayload(Payload const& payload);

	Builder& FromDecodedPack(std::span<std::uint8_t const> buffer);
	Builder& FromEncodedPack(std::string_view pack);

    Parcel&& Build();
    OptionalParcel ValidatedBuild();

	UT_SupportMethod(Builder& MakeClusterMessage());
	UT_SupportMethod(Builder& MakeNetworkMessage());

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
