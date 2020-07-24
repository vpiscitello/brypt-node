//------------------------------------------------------------------------------------------------
// File: Message.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "MessageSecurity.hpp"
#include "MessageTypes.hpp"
#include "../Utilities/NodeUtils.hpp"
#include "../Utilities/TimeUtils.hpp"
#include "../Components/Command/CommandDefinitions.hpp"
//------------------------------------------------------------------------------------------------
#include <optional>
#include <string>
#include <string_view>
//------------------------------------------------------------------------------------------------

class CMessageBuilder;

//------------------------------------------------------------------------------------------------

class CMessage {
public:
	enum class ValidationStatus : std::uint8_t { Success, Error };

	CMessage();
	CMessage(CMessage const& other);

	// CMessageBuilder {
    friend class CMessageBuilder;
    static CMessageBuilder Builder();
	// } CMessageBuilder

	CMessageContext const& GetMessageContext() const;
	NodeUtils::NodeIdType const& GetSource() const;
	NodeUtils::NodeIdType const& GetDestination() const;
	std::optional<NodeUtils::ObjectIdType> GetAwaitingKey() const;
	Command::Type GetCommandType() const;
	std::uint32_t GetPhase() const;
	Message::Buffer const& GetData() const;
	std::optional<Message::Buffer> GetDecryptedData() const;
	TimeUtils::Timepoint const& GetSystemTimepoint() const;
	NodeUtils::NetworkNonce GetNonce() const;
	std::string GetPack() const;

	ValidationStatus Validate() const;

	constexpr static std::size_t FixedPackSize()
	{
		std::size_t size = 0;
		size += sizeof(m_source);
		size += sizeof(m_destination);
		size += sizeof(m_optBoundAwaitingKey->first);
		size += sizeof(m_optBoundAwaitingKey->second);
		size += sizeof(m_command);
		size += sizeof(m_phase);
		size += sizeof(std::uint16_t);
		size += sizeof(m_nonce);
		size += sizeof(std::uint64_t);
		size += MessageSecurity::TokenSize;
		return size;
	}

private:
	CMessageContext m_context;

	NodeUtils::NodeIdType m_source;	// ID of the sending node
	NodeUtils::NodeIdType m_destination;	// ID of the receiving node
	std::optional<Message::BoundAwaitingKey> m_optBoundAwaitingKey;	// ID bound to the source or destination on a passdown message

	Command::Type m_command;	// Command type to be run
	std::uint8_t m_phase;	// Phase of the Command state

	Message::Buffer m_data;	// Primary message content

	NodeUtils::NetworkNonce m_nonce;	// Current message nonce

	TimeUtils::Timepoint m_timepoint;	// The timepoint that message was created

};

//------------------------------------------------------------------------------------------------

