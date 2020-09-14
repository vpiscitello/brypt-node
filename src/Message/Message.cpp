//------------------------------------------------------------------------------------------------
// File: Message.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "Message.hpp"
#include "MessageBuilder.hpp"
#include "PackUtils.hpp"
#include "../BryptIdentifier/BryptIdentifier.hpp"
//------------------------------------------------------------------------------------------------

CMessage::CMessage()
	: m_context()
	, m_source()
	, m_destination()
	, m_optBoundAwaitingKey()
	, m_command()
	, m_phase()
	, m_data()
	, m_nonce()
	, m_timepoint(TimeUtils::GetSystemTimepoint())
{
}

//------------------------------------------------------------------------------------------------

CMessage::CMessage(CMessage const& other)
	: m_context(other.m_context)
	, m_source(other.m_source)
	, m_destination(other.m_destination)
	, m_optBoundAwaitingKey(other.m_optBoundAwaitingKey)
	, m_command(other.m_command)
	, m_phase(other.m_phase)
	, m_data(other.m_data)
	, m_nonce(other.m_nonce)
	, m_timepoint(other.m_timepoint)
{
}
//------------------------------------------------------------------------------------------------

CMessageBuilder CMessage::Builder()
{
	return CMessageBuilder{};
}

//------------------------------------------------------------------------------------------------

CMessageContext const& CMessage::GetMessageContext() const
{
	return m_context;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const& CMessage::GetSource() const
{
	return m_source;
}

//------------------------------------------------------------------------------------------------

BryptIdentifier::CContainer const& CMessage::GetDestination() const
{
	return m_destination;
}

//------------------------------------------------------------------------------------------------

std::optional<Await::TrackerKey> CMessage::GetAwaitingKey() const
{
	if (!m_optBoundAwaitingKey) {
		return {};
	}
	return m_optBoundAwaitingKey->second;
}

//------------------------------------------------------------------------------------------------

Command::Type CMessage::GetCommandType() const
{
	return m_command;
}

//------------------------------------------------------------------------------------------------

std::uint32_t CMessage::GetPhase() const
{
	return m_phase;
}

//------------------------------------------------------------------------------------------------

Message::Buffer const& CMessage::GetData() const
{
	return m_data;
}

//------------------------------------------------------------------------------------------------

std::optional<Message::Buffer> CMessage::GetDecryptedData() const
{
    auto const decrypted = MessageSecurity::Decrypt(m_data, m_data.size(), m_nonce);
	return decrypted;
}

//------------------------------------------------------------------------------------------------

TimeUtils::Timepoint const& CMessage::GetSystemTimepoint() const
{
	return m_timepoint;
}

//------------------------------------------------------------------------------------------------

NodeUtils::NetworkNonce CMessage::GetNonce() const
{
	return m_nonce;
}

//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: Pack the Message class values into a single raw string.
//------------------------------------------------------------------------------------------------
std::string CMessage::GetPack() const
{
	std::uint32_t const size = FixedPackSize() + 
		m_source.NetworkRepresentationSize() +
		m_destination.NetworkRepresentationSize() +
		m_data.size();

	Message::Buffer buffer;
	buffer.reserve(size);

	PackUtils::PackChunk(buffer, m_source.GetNetworkRepresentation());
	PackUtils::PackChunk(buffer, BryptIdentifier::TerminatorByte);

	PackUtils::PackChunk(buffer, m_destination.GetNetworkRepresentation());
	PackUtils::PackChunk(buffer, BryptIdentifier::TerminatorByte);

	if (m_optBoundAwaitingKey) {
		PackUtils::PackChunk(buffer, m_optBoundAwaitingKey->first);
		PackUtils::PackChunk(buffer, m_optBoundAwaitingKey->second);
	} else {
		PackUtils::PackChunk(buffer, Message::AwaitBinding::None);
	}
	PackUtils::PackChunk(buffer, m_command);
	PackUtils::PackChunk(buffer, m_phase);
	PackUtils::PackChunk(buffer, m_nonce);
	PackUtils::PackChunk(buffer, static_cast<std::uint16_t>(m_data.size()));
	PackUtils::PackChunk(buffer, m_data);
	PackUtils::PackChunk(buffer, TimeUtils::TimepointToTimePeriod(m_timepoint));

	auto const optSignature = MessageSecurity::HMAC(buffer, buffer.size());
	if (!optSignature) {
		return "";
	}

	buffer.insert(buffer.end(), optSignature->begin(), optSignature->end());
	return PackUtils::Z85Encode(buffer);
}

//------------------------------------------------------------------------------------------------

CMessage::ValidationStatus CMessage::Validate() const
{	
	// A message must have a valid brypt source identifier attached
	if (!m_source.IsValid()) {
		return ValidationStatus::Error;
	}

	// A message must have a valid brypt destination identifier attached
	if (!m_destination.IsValid()) {
		return ValidationStatus::Error;
	}

	// A message must identify a valid brypt command
	if (m_command == static_cast<Command::Type>(Command::Type::Invalid)) {
		return ValidationStatus::Error;
	}

	// A message must identify the time it was created
	if (m_timepoint == TimeUtils::Timepoint()) {
		return ValidationStatus::Error;
	}

	return ValidationStatus::Success;
}

//------------------------------------------------------------------------------------------------
