//----------------------------------------------------------------------------------------------------------------------
// File: NetworkMessage.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "MessageHeader.hpp"
#include "MessageTypes.hpp"
#include "BryptIdentifier/BryptIdentifier.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <optional>
#include <span>
#include <string>
#include <string_view>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace Message {
namespace Network {
//----------------------------------------------------------------------------------------------------------------------

enum class Type : std::uint8_t { Invalid, Handshake, HeartbeatRequest, HeartbeatResponse };

//----------------------------------------------------------------------------------------------------------------------
} // Network namespace 
} // Message namespace
//----------------------------------------------------------------------------------------------------------------------

class NetworkBuilder;

//----------------------------------------------------------------------------------------------------------------------

class NetworkMessage {
public:
	NetworkMessage();
	NetworkMessage(NetworkMessage const& other);

	// NetworkBuilder {
	friend class NetworkBuilder;
	static NetworkBuilder Builder();
	// } NetworkBuilder

	MessageContext const& GetContext() const;

	MessageHeader const& GetMessageHeader() const;
	Node::Identifier const& GetSourceIdentifier() const;
	Message::Destination GetDestinationType() const;
	std::optional<Node::Identifier> const& GetDestinationIdentifier() const;
	Message::Network::Type GetMessageType() const;
	Message::Buffer const& GetPayload() const;

    std::size_t GetPackSize() const;
	std::string GetPack() const;
	Message::ValidationStatus Validate() const;

private:
	constexpr std::size_t FixedPackSize() const;

	MessageContext m_context; // The internal message context of the message
	MessageHeader m_header; // The required message header 

	Message::Network::Type m_type;
	Message::Buffer m_payload;
};

//----------------------------------------------------------------------------------------------------------------------

class NetworkBuilder {
public:
	using OptionalMessage = std::optional<NetworkMessage>;

	NetworkBuilder();

	NetworkBuilder& SetMessageContext(MessageContext const& context);
	NetworkBuilder& SetSource(Node::Identifier const& identifier);
	NetworkBuilder& SetSource(Node::Internal::Identifier::Type const& identifier);
	NetworkBuilder& SetSource(std::string_view identifier);
	NetworkBuilder& SetDestination(Node::Identifier const& identifier);
	NetworkBuilder& SetDestination(Node::Internal::Identifier::Type const& identifier);
	NetworkBuilder& SetDestination(std::string_view identifier);
	NetworkBuilder& MakeHandshakeMessage();
	NetworkBuilder& MakeHeartbeatRequest();
	NetworkBuilder& MakeHeartbeatResponse();
	NetworkBuilder& SetPayload(std::string_view buffer);
	NetworkBuilder& SetPayload(std::span<std::uint8_t const> buffer);

	NetworkBuilder& FromDecodedPack(std::span<std::uint8_t const> buffer);
	NetworkBuilder& FromEncodedPack(std::string_view pack);

    NetworkMessage&& Build();
    OptionalMessage ValidatedBuild();

private:
	void Unpack(std::span<std::uint8_t const> buffer);
	void UnpackExtensions(
        std::span<std::uint8_t const>::iterator& begin,
        std::span<std::uint8_t const>::iterator const& end);

    NetworkMessage m_message;
};

//----------------------------------------------------------------------------------------------------------------------
