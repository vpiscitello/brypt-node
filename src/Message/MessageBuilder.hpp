//------------------------------------------------------------------------------------------------
// File: MessageBuilder.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "Message.hpp"
#include "../Utilities/ReservedIdentifiers.hpp"
//------------------------------------------------------------------------------------------------
#include <optional>
//------------------------------------------------------------------------------------------------

using OptionalMessage = std::optional<CMessage>;

//------------------------------------------------------------------------------------------------

class CMessageBuilder {
public:
	CMessageBuilder();

	CMessageBuilder& SetMessageContext(CMessageContext const& context);
	CMessageBuilder& SetSource(NodeUtils::NodeIdType id);
	CMessageBuilder& SetDestination(NodeUtils::NodeIdType id);
	CMessageBuilder& SetDestination(ReservedIdentifiers id);
	CMessageBuilder& BindAwaitingKey(Message::AwaitBinding binding, NodeUtils::ObjectIdType key);
	CMessageBuilder& SetCommand(Command::Type type, std::uint8_t phase);
	CMessageBuilder& SetData(std::string_view data, NodeUtils::NetworkNonce nonce = 0);
	CMessageBuilder& SetData(Message::Buffer const& buffer, NodeUtils::NetworkNonce nonce = 0);

	CMessageBuilder& FromPack(Message::Buffer const& buffer);
	CMessageBuilder& FromPack(std::string_view pack);

    CMessage&& Build();
    OptionalMessage ValidatedBuild();

private:
	void Unpack(Message::Buffer const& buffer);

    CMessage m_message;

};

//------------------------------------------------------------------------------------------------

