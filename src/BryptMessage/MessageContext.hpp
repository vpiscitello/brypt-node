//----------------------------------------------------------------------------------------------------------------------
// File: MessageContext.hpp
// Description:
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
#include "Components/Network/EndpointIdentifier.hpp"
#include "Components/Network/Protocol.hpp"
#include "Components/Security/SecurityTypes.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <span>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: A class to describe various information about the message context local to the 
// node itself. This information is not a part of the message, but determined by the endpoint
// it was received or transmitted by. Currently, this is being used to identify which endpoint 
// the a response should be forwarded to. This is needed because it is valid for a peer to be 
// connected via multiple endpoints (e.g. connected as a server and a client).
//----------------------------------------------------------------------------------------------------------------------
class MessageContext {
public:
	MessageContext();
	MessageContext(Network::Endpoint::Identifier identifier, Network::Protocol protocol);

	[[nodiscard]] Network::Endpoint::Identifier GetEndpointIdentifier() const;
	[[nodiscard]] Network::Protocol GetEndpointProtocol() const;

	[[nodiscard]] bool HasSecurityHandlers() const;

	void BindEncryptionHandlers(
		Security::Encryptor const& encryptor, Security::Decryptor const& decryptor);
	
	void BindSignatureHandlers(
		Security::Signator const& signator,
		Security::Verifier const& verifier,
		Security::SignatureSizeGetter const& getter);

	[[nodiscard]] Security::Encryptor::result_type Encrypt(
		std::span<std::uint8_t const> buffer, TimeUtils::Timestamp const& timestamp) const;
	[[nodiscard]] Security::Decryptor::result_type Decrypt(
		std::span<std::uint8_t const> buffer, TimeUtils::Timestamp const& timestamp) const;
	[[nodiscard]] Security::Signator::result_type Sign(Message::Buffer& buffer) const;
	[[nodiscard]] Security::Verifier::result_type Verify(
		std::span<std::uint8_t const> buffer) const;
	[[nodiscard]] std::size_t GetSignatureSize() const;

private:
	Network::Endpoint::Identifier m_endpointIdentifier;
	Network::Protocol m_endpointProtocol;

	Security::Encryptor m_encryptor;
	Security::Decryptor m_decryptor;
	Security::Signator m_signator;
	Security::Verifier m_verifier;
	Security::SignatureSizeGetter m_getSignatureSize;
};

//----------------------------------------------------------------------------------------------------------------------
