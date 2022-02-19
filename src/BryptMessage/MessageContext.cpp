//----------------------------------------------------------------------------------------------------------------------
// File: MessageContext.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Message::Context::Context()
	: m_endpointIdentifier(Network::Endpoint::InvalidIdentifier)
	, m_endpointProtocol(Network::Protocol::Invalid)
	, m_encryptor()
	, m_decryptor()
	, m_signator()
	, m_verifier()
	, m_getSignatureSize()
{
}

//----------------------------------------------------------------------------------------------------------------------

Message::Context::Context(
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

//----------------------------------------------------------------------------------------------------------------------

Network::Endpoint::Identifier Message::Context::GetEndpointIdentifier() const
{
	return m_endpointIdentifier;
}

//----------------------------------------------------------------------------------------------------------------------

Network::Protocol Message::Context::GetEndpointProtocol() const
{
	return m_endpointProtocol;
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Context::HasSecurityHandlers() const
{
	return (m_encryptor && m_decryptor && m_signator && m_verifier && m_getSignatureSize);
}

//----------------------------------------------------------------------------------------------------------------------

void Message::Context::BindEncryptionHandlers(
	Security::Encryptor const& encryptor, Security::Decryptor const& decryptor)
{
	m_encryptor = encryptor;
	m_decryptor = decryptor;
}

//----------------------------------------------------------------------------------------------------------------------

void Message::Context::BindSignatureHandlers(
	Security::Signator const& signator,
	Security::Verifier const& verifier,
	Security::SignatureSizeGetter const& getter)
{
	m_signator = signator;
	m_verifier = verifier;
	m_getSignatureSize = getter;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Encryptor::result_type Message::Context::Encrypt(
	std::span<std::uint8_t const> buffer, TimeUtils::Timestamp const& timestamp) const
{
    assert(timestamp.count() >= 0);
	return m_encryptor(buffer, static_cast<std::uint64_t>(timestamp.count()));
}

//----------------------------------------------------------------------------------------------------------------------

Security::Decryptor::result_type Message::Context::Decrypt(
	std::span<std::uint8_t const> buffer, TimeUtils::Timestamp const& timestamp) const
{
    assert(timestamp.count() >= 0);
	return m_decryptor(buffer, static_cast<std::uint64_t>(timestamp.count()));
}

//----------------------------------------------------------------------------------------------------------------------

Security::Signator::result_type Message::Context::Sign(Message::Buffer& buffer) const
{
	return m_signator(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

Security::Verifier::result_type Message::Context::Verify(std::span<std::uint8_t const> buffer) const
{
	return m_verifier(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Context::GetSignatureSize() const
{
	return m_getSignatureSize();
}

//----------------------------------------------------------------------------------------------------------------------
