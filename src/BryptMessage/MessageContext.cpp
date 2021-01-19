//------------------------------------------------------------------------------------------------
// File: MessageContext.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

MessageContext::MessageContext()
	: m_endpointIdentifier(Network::Endpoint::InvalidIdentifier)
	, m_endpointProtocol(Network::Protocol::Invalid)
	, m_encryptor()
	, m_decryptor()
	, m_signator()
	, m_verifier()
	, m_getSignatureSize()
{
}

//------------------------------------------------------------------------------------------------

MessageContext::MessageContext(
	Network::Endpoint::Identifier identifier,
	Network::Protocol protocol)
	: m_endpointIdentifier(identifier)
	, m_endpointProtocol(protocol)
	, m_encryptor()
	, m_decryptor()
	, m_signator()
	, m_verifier()
	, m_getSignatureSize()
{
}

//------------------------------------------------------------------------------------------------

Network::Endpoint::Identifier MessageContext::GetEndpointIdentifier() const
{
	return m_endpointIdentifier;
}

//------------------------------------------------------------------------------------------------

Network::Protocol MessageContext::GetEndpointProtocol() const
{
	return m_endpointProtocol;
}

//------------------------------------------------------------------------------------------------

bool MessageContext::HasSecurityHandlers() const
{
	return (m_encryptor && m_decryptor && m_signator && m_verifier && m_getSignatureSize);
}

//------------------------------------------------------------------------------------------------

void MessageContext::BindEncryptionHandlers(
	Security::Encryptor const& encryptor, Security::Decryptor const& decryptor)
{
	m_encryptor = encryptor;
	m_decryptor = decryptor;
}

//------------------------------------------------------------------------------------------------

void MessageContext::BindSignatureHandlers(
	Security::Signator const& signator,
	Security::Verifier const& verifier,
	Security::SignatureSizeGetter const& getter)
{
	m_signator = signator;
	m_verifier = verifier;
	m_getSignatureSize = getter;
}

//------------------------------------------------------------------------------------------------

Security::Encryptor::result_type MessageContext::Encrypt(
	std::span<std::uint8_t const> buffer, TimeUtils::Timestamp const& timestamp) const
{
	return m_encryptor(buffer, timestamp.count());
}

//------------------------------------------------------------------------------------------------

Security::Decryptor::result_type MessageContext::Decrypt(
	std::span<std::uint8_t const> buffer, TimeUtils::Timestamp const& timestamp) const
{
	return m_decryptor(buffer, timestamp.count());
}

//------------------------------------------------------------------------------------------------

Security::Signator::result_type MessageContext::Sign(Message::Buffer& buffer) const
{
	return m_signator(buffer);
}

//------------------------------------------------------------------------------------------------

Security::Verifier::result_type MessageContext::Verify(
	std::span<std::uint8_t const> buffer) const
{
	return m_verifier(buffer);
}

//------------------------------------------------------------------------------------------------

std::size_t MessageContext::GetSignatureSize() const
{
	return m_getSignatureSize();
}

//------------------------------------------------------------------------------------------------
