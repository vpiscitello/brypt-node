//----------------------------------------------------------------------------------------------------------------------
// File: PlatformMessage.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "MessageHeader.hpp"
#include "MessageTypes.hpp"
#include "ShareablePack.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
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

class Message::Platform::Parcel {
public:
	Parcel();
	Parcel(Parcel const& other);

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
	Buffer const& GetPayload() const;

	std::size_t GetPackSize() const;
	std::string GetPack() const;
	ShareablePack GetShareablePack() const;
	ValidationStatus Validate() const;

private:
	constexpr std::size_t FixedPackSize() const;

	Context m_context; // The internal message context of the message
	Header m_header; // The required message header 

	ParcelType m_type;
	Buffer m_payload;
};

//----------------------------------------------------------------------------------------------------------------------

class Message::Platform::Builder {
public:
	using OptionalParcel = std::optional<Parcel>;

	Builder();

	Node::Identifier const& GetSource() const;
	std::optional<Node::Identifier> const& GetDestination() const;
	
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
	Builder& SetPayload(std::string_view buffer);
	Builder& SetPayload(std::span<std::uint8_t const> buffer);

	Builder& FromDecodedPack(std::span<std::uint8_t const> buffer);
	Builder& FromEncodedPack(std::string_view pack);

    Parcel&& Build();
    OptionalParcel ValidatedBuild();

private:
	void Unpack(std::span<std::uint8_t const> buffer);
	void UnpackExtensions(
        std::span<std::uint8_t const>::iterator& begin, std::span<std::uint8_t const>::iterator const& end);

    Parcel m_parcel;
	bool m_hasStageFailure;
};

//----------------------------------------------------------------------------------------------------------------------