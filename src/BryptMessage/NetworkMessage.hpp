//------------------------------------------------------------------------------------------------
// File: NetworkMessage.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "MessageHeader.hpp"
#include "MessageTypes.hpp"
#include "../BryptIdentifier/BryptIdentifier.hpp"
//------------------------------------------------------------------------------------------------
#include <optional>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
namespace Message {
namespace Network {
//------------------------------------------------------------------------------------------------

enum class Type : std::uint8_t {
	Invalid, Handshake, HeartbeatRequest, HeartbeatResponse };

//------------------------------------------------------------------------------------------------
} // Network namespace 
} // Message namespace
//------------------------------------------------------------------------------------------------

class CNetworkBuilder;

//------------------------------------------------------------------------------------------------

class CNetworkMessage {
public:
	CNetworkMessage();
	CNetworkMessage(CNetworkMessage const& other);

	// CNetworkBuilder {
	friend class CNetworkBuilder;
	static CNetworkBuilder Builder();
	// } CNetworkBuilder

	CMessageContext const& GetContext() const;

	CMessageHeader const& GetMessageHeader() const;
	BryptIdentifier::CContainer const& GetSourceIdentifier() const;
	Message::Destination GetDestinationType() const;
	std::optional<BryptIdentifier::CContainer> const& GetDestinationIdentifier() const;
	Message::Network::Type GetMessageType() const;
	Message::Buffer const& GetPayload() const;

    std::uint32_t GetPackSize() const;
	std::string GetPack() const;
	Message::ValidationStatus Validate() const;

private:
	constexpr std::uint32_t FixedPackSize() const;

	CMessageContext m_context; // The internal message context of the message
	CMessageHeader m_header; // The required message header 

	Message::Network::Type m_type;
	Message::Buffer m_payload;

};

//------------------------------------------------------------------------------------------------

class CNetworkBuilder {
public:
	using OptionalMessage = std::optional<CNetworkMessage>;

	CNetworkBuilder();

	CNetworkBuilder& SetMessageContext(CMessageContext const& context);
	CNetworkBuilder& SetSource(BryptIdentifier::CContainer const& identifier);
	CNetworkBuilder& SetSource(BryptIdentifier::Internal::Type const& identifier);
	CNetworkBuilder& SetSource(std::string_view identifier);
	CNetworkBuilder& SetDestination(BryptIdentifier::CContainer const& identifier);
	CNetworkBuilder& SetDestination(BryptIdentifier::Internal::Type const& identifier);
	CNetworkBuilder& SetDestination(std::string_view identifier);
	CNetworkBuilder& MakeHandshakeMessage();
	CNetworkBuilder& MakeHeartbeatRequest();
	CNetworkBuilder& MakeHeartbeatResponse();
	CNetworkBuilder& SetPayload(std::string_view data);
	CNetworkBuilder& SetPayload(Message::Buffer const& buffer);

	CNetworkBuilder& FromDecodedPack(Message::Buffer const& buffer);
	CNetworkBuilder& FromEncodedPack(std::string_view pack);

    CNetworkMessage&& Build();
    OptionalMessage ValidatedBuild();

private:
	void Unpack(Message::Buffer const& buffer);
	void UnpackExtensions(
        Message::Buffer::const_iterator& begin,
        Message::Buffer::const_iterator const& end);

    CNetworkMessage m_message;

};

//------------------------------------------------------------------------------------------------
