//----------------------------------------------------------------------------------------------------------------------
// File: MessageContext.cpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#include "MessageContext.hpp"
#include "Components/Peer/Proxy.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cassert>
//----------------------------------------------------------------------------------------------------------------------

Message::Context::Context()
	: m_wpProxy()
	, m_endpointIdentifier(Network::Endpoint::InvalidIdentifier)
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
	std::weak_ptr<Peer::Proxy> const& wpProxy, Network::Endpoint::Identifier identifier, Network::Protocol protocol)
	: m_wpProxy(wpProxy)
	, m_endpointIdentifier(identifier)
	, m_endpointProtocol(protocol)
	, m_encryptor()
	, m_decryptor()
	, m_signator()
	, m_verifier()
	, m_getSignatureSize()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Context::operator==(Context const& other) const
{
	return m_endpointIdentifier == other.m_endpointIdentifier && m_endpointProtocol == other.m_endpointProtocol;
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

std::weak_ptr<Peer::Proxy> const& Message::Context::GetProxy() const
{
	return m_wpProxy;
}

//----------------------------------------------------------------------------------------------------------------------

bool Message::Context::HasSecurityHandlers() const
{
	return (m_encryptor && m_decryptor && m_signator && m_verifier && m_getSignatureSize);
}

//----------------------------------------------------------------------------------------------------------------------

void Message::Context::BindEncryptionHandlers(
	Security::Encryptor const& encryptor,
	Security::Decryptor const& decryptor,
	Security::EncryptedSizeGetter const& encryptedSizeGetter)
{
	m_encryptor = encryptor;
	m_decryptor = decryptor;
	m_getEncryptedSize = encryptedSizeGetter;
}

//----------------------------------------------------------------------------------------------------------------------

void Message::Context::BindSignatureHandlers(
	Security::Signator const& signator,
	Security::Verifier const& verifier,
	Security::SignatureSizeGetter const& signatureSizeGetter)
{
	m_signator = signator;
	m_verifier = verifier;
	m_getSignatureSize = signatureSizeGetter;
}

//----------------------------------------------------------------------------------------------------------------------

Security::Encryptor::result_type Message::Context::Encrypt(Security::ReadableView plaintext, Security::Buffer& destination) const
{
	if (m_wpProxy.expired()) { return {}; }
	return m_encryptor(plaintext, destination);
}

//----------------------------------------------------------------------------------------------------------------------

Security::Decryptor::result_type Message::Context::Decrypt(Security::ReadableView ciphertext) const
{
	if (m_wpProxy.expired()) { return {}; }
	return m_decryptor(ciphertext);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Context::GetEncryptedSize(std::size_t size) const
{
	if (m_wpProxy.expired()) { return std::numeric_limits<std::size_t>::min(); }
	return m_getEncryptedSize(size);
}

//----------------------------------------------------------------------------------------------------------------------

Security::Signator::result_type Message::Context::Sign(Message::Buffer& buffer) const
{
	if (m_wpProxy.expired()) { return false; }
	return m_signator(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

Security::Verifier::result_type Message::Context::Verify(std::span<std::uint8_t const> buffer) const
{
	if (m_wpProxy.expired()) { return Security::Verifier::result_type::Failed; }
	return m_verifier(buffer);
}

//----------------------------------------------------------------------------------------------------------------------

std::size_t Message::Context::GetSignatureSize() const
{
	if (m_wpProxy.expired()) { return std::numeric_limits<std::size_t>::min(); }
	return m_getSignatureSize();
}

//----------------------------------------------------------------------------------------------------------------------

template <>
void Message::Context::BindProxy<InvokeContext::Test>(std::weak_ptr<Peer::Proxy> const& wpProxy)
{
	m_wpProxy = wpProxy;
}

//----------------------------------------------------------------------------------------------------------------------
