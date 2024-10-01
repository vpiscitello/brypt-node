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
#include "Utilities/InvokeContext.hpp"
#include "Utilities/TimeUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <span>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

namespace Peer { class Proxy; }

//----------------------------------------------------------------------------------------------------------------------
namespace Message {
//----------------------------------------------------------------------------------------------------------------------

class Context;

//----------------------------------------------------------------------------------------------------------------------
} // Message namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Description: A class to describe various information about the message context local to the 
// node itself. This information is not a part of the message, but determined by the endpoint
// it was received or transmitted by. Currently, this is being used to identify which endpoint 
// the a response should be forwarded to. This is needed because it is valid for a peer to be 
// connected via multiple endpoints (e.g. connected as a server and a client).
//----------------------------------------------------------------------------------------------------------------------
class Message::Context {
public:
	Context();
	Context(
		std::weak_ptr<Peer::Proxy> const& wpProxy, Network::Endpoint::Identifier identifier, Network::Protocol protocol);

	[[nodiscard]] bool operator==(Context const& other) const;

	[[nodiscard]] Network::Endpoint::Identifier GetEndpointIdentifier() const;
	[[nodiscard]] Network::Protocol GetEndpointProtocol() const;
	[[nodiscard]] std::weak_ptr<Peer::Proxy> const& GetProxy() const;
	[[nodiscard]] bool HasSecurityHandlers() const;

	void BindEncryptionHandlers(
		Security::Encryptor const& encryptor,
		Security::Decryptor const& decryptor,
		Security::EncryptedSizeGetter const& encryptedSizeGetter);
	
	void BindSignatureHandlers(
		Security::Signator const& signator,
		Security::Verifier const& verifier,
		Security::SignatureSizeGetter const& signatureSizeGetter);

	[[nodiscard]] Security::Encryptor::result_type Encrypt(Security::ReadableView plaintext, Security::Buffer& destination) const;
	[[nodiscard]] Security::Decryptor::result_type Decrypt(Security::ReadableView ciphertext) const;
	[[nodiscard]] std::size_t GetEncryptedSize(std::size_t size) const;
	[[nodiscard]] Security::Signator::result_type Sign(Message::Buffer& buffer) const;
	[[nodiscard]] Security::Verifier::result_type Verify(std::span<std::uint8_t const> buffer) const;
	[[nodiscard]] std::size_t GetSignatureSize() const;

	UT_SupportMethod(void BindProxy(std::weak_ptr<Peer::Proxy> const& wpProxy));

private:
	std::weak_ptr<Peer::Proxy> m_wpProxy;

	Network::Endpoint::Identifier m_endpointIdentifier;
	Network::Protocol m_endpointProtocol;

	Security::Encryptor m_encryptor;
	Security::Decryptor m_decryptor;
	Security::EncryptedSizeGetter m_getEncryptedSize;
	Security::Signator m_signator;
	Security::Verifier m_verifier;
	Security::SignatureSizeGetter m_getSignatureSize;
};

//----------------------------------------------------------------------------------------------------------------------
