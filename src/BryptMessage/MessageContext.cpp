//------------------------------------------------------------------------------------------------
// File: MessageContext.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
//------------------------------------------------------------------------------------------------
#include <cassert>
//------------------------------------------------------------------------------------------------

MessageContext::MessageContext()
	: m_endpointIdentifier(Endpoints::InvalidEndpointIdentifier)
	, m_endpointTechnology(Endpoints::TechnologyType::Invalid)
	, m_encryptor()
	, m_decryptor()
	, m_signator()
	, m_verifier()
	, m_getSignatureSize()
{
}

//------------------------------------------------------------------------------------------------

MessageContext::MessageContext(
	Endpoints::EndpointIdType identifier,
	Endpoints::TechnologyType technology)
	: m_endpointIdentifier(identifier)
	, m_endpointTechnology(technology)
	, m_encryptor()
	, m_decryptor()
	, m_signator()
	, m_verifier()
	, m_getSignatureSize()
{
}

//------------------------------------------------------------------------------------------------

Endpoints::EndpointIdType CMessageContext::GetEndpointIdentifier() const
{
	return m_endpointIdentifier;
}

//------------------------------------------------------------------------------------------------

Endpoints::TechnologyType CMessageContext::GetEndpointTechnology() const
{
	return m_endpointTechnology;
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
