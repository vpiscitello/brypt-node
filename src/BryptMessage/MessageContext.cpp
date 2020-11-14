//------------------------------------------------------------------------------------------------
// File: MessageContext.cpp
// Description:
//------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
//------------------------------------------------------------------------------------------------

CMessageContext::CMessageContext()
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

CMessageContext::CMessageContext(
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

bool CMessageContext::HasSecurityHandlers() const
{
	return (m_encryptor && m_decryptor && m_signator && m_verifier && m_getSignatureSize);
}

//------------------------------------------------------------------------------------------------

void CMessageContext::BindEncryptionHandlers(
	Security::Encryptor const& encryptor, Security::Decryptor const& decryptor)
{
	m_encryptor = encryptor;
	m_decryptor = decryptor;
}

//------------------------------------------------------------------------------------------------

void CMessageContext::BindSignatureHandlers(
	Security::Signator const& signator,
	Security::Verifier const& verifier,
	Security::SignatureSizeGetter const& getter)
{
	m_signator = signator;
	m_verifier = verifier;
	m_getSignatureSize = getter;
}

//------------------------------------------------------------------------------------------------

Security::Encryptor::result_type CMessageContext::Encrypt(
	Message::Buffer const& buffer, TimeUtils::Timestamp const& timestamp) const
{
	if (!m_encryptor) {
		return {};
	}
	return m_encryptor(buffer, buffer.size(), timestamp.count());
}

//------------------------------------------------------------------------------------------------

Security::Decryptor::result_type CMessageContext::Decrypt(
	Message::Buffer const& buffer, TimeUtils::Timestamp const& timestamp) const
{
	if (!m_decryptor) {
		return {};
	}
	return m_decryptor(buffer, buffer.size(), timestamp.count());
}

//------------------------------------------------------------------------------------------------

Security::Signator::result_type CMessageContext::Sign(Message::Buffer& buffer) const
{
	if (!m_signator) {
		return -1;
	}
	return m_signator(buffer);
}

//------------------------------------------------------------------------------------------------

Security::Verifier::result_type CMessageContext::Verify(Message::Buffer const& buffer) const
{
	if (!m_verifier) {
		return Security::VerificationStatus::Failed;
	}
	return m_verifier(buffer);
}

//------------------------------------------------------------------------------------------------

std::int32_t CMessageContext::GetSignatureSize() const
{
	if (!m_getSignatureSize) {
		return -1;
	}
	return m_getSignatureSize();
}

//------------------------------------------------------------------------------------------------
