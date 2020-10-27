//------------------------------------------------------------------------------------------------
// File: HandshakeMessage.hpp
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
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
} // Message namespace
//------------------------------------------------------------------------------------------------

class CHandshakeBuilder;

//------------------------------------------------------------------------------------------------

class CHandshakeMessage {
public:
	CHandshakeMessage();
	CHandshakeMessage(CHandshakeMessage const& other);

	// CHandshakeBuilder {
	friend class CHandshakeBuilder;
	static CHandshakeBuilder Builder();
	// } CHandshakeBuilder

	CMessageContext const& GetMessageContext() const;

	CMessageHeader const& GetMessageHeader() const;
	BryptIdentifier::CContainer const& GetSourceIdentifier() const;
	Message::Destination GetDestinationType() const;
	std::optional<BryptIdentifier::CContainer> const& GetDestinationIdentifier() const;
	Message::Buffer GetData() const;

    std::uint32_t GetPackSize() const;
	std::string GetPack() const;
	Message::ValidationStatus Validate() const;

private:
	constexpr std::uint32_t FixedPackSize() const;

	CMessageContext m_context; // The internal message context of the message
	CMessageHeader m_header; // The required message header 

	Message::Buffer m_data;	// The command payload

};

//------------------------------------------------------------------------------------------------

class CHandshakeBuilder {
public:
	using OptionalMessage = std::optional<CHandshakeMessage>;

	CHandshakeBuilder();

	CHandshakeBuilder& SetMessageContext(CMessageContext const& context);
	CHandshakeBuilder& SetSource(BryptIdentifier::CContainer const& identifier);
	CHandshakeBuilder& SetSource(BryptIdentifier::Internal::Type const& identifier);
	CHandshakeBuilder& SetSource(std::string_view identifier);
	CHandshakeBuilder& SetDestination(BryptIdentifier::CContainer const& identifier);
	CHandshakeBuilder& SetDestination(BryptIdentifier::Internal::Type const& identifier);
	CHandshakeBuilder& SetDestination(std::string_view identifier);
	CHandshakeBuilder& SetData(std::string_view data);
	CHandshakeBuilder& SetData(Message::Buffer const& buffer);

	CHandshakeBuilder& FromDecodedPack(Message::Buffer const& buffer);
	CHandshakeBuilder& FromEncodedPack(std::string_view pack);

    CHandshakeMessage&& Build();
    OptionalMessage ValidatedBuild();

private:
	void Unpack(Message::Buffer const& buffer);
	void UnpackExtensions(
        Message::Buffer::const_iterator& begin,
        Message::Buffer::const_iterator const& end);

    CHandshakeMessage m_message;

};

//------------------------------------------------------------------------------------------------
