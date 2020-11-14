//------------------------------------------------------------------------------------------------
// File: MessageContext.hpp
// Description:
//------------------------------------------------------------------------------------------------
#pragma once
//------------------------------------------------------------------------------------------------
#include "MessageTypes.hpp"
#include "../Components/Endpoints/EndpointIdentifier.hpp"
#include "../Components/Endpoints/TechnologyType.hpp"
#include "../Components/Security/SecurityTypes.hpp"
#include "../Utilities/TimeUtils.hpp"
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Description: A class to describe various information about the message context local to the 
// node itself. This information is not a part of the message, but determined by the endpoint
// it was received or transmitted by. Currently, this is being used to identify which endpoint 
// the a response should be forwarded to. This is needed because it is valid for a peer to be 
// connected via multiple endpoints (e.g. connected as a server and a client).
//------------------------------------------------------------------------------------------------
class CMessageContext {
public:
	CMessageContext();
	CMessageContext(Endpoints::EndpointIdType identifier, Endpoints::TechnologyType technology);

	Endpoints::EndpointIdType GetEndpointIdentifier() const;
	Endpoints::TechnologyType GetEndpointTechnology() const;

	bool HasSecurityHandlers() const;

	void BindEncryptionHandlers(
		Security::Encryptor const& encryptor, Security::Decryptor const& decryptor);
	
	void BindSignatureHandlers(
		Security::Signator const& signator,
		Security::Verifier const& verifier,
		Security::SignatureSizeGetter const& getter);

	Security::Encryptor::result_type Encrypt(
		Message::Buffer const& buffer, TimeUtils::Timestamp const& timestamp) const;
	Security::Decryptor::result_type Decrypt(
		Message::Buffer const& buffer, TimeUtils::Timestamp const& timestamp) const;
	Security::Signator::result_type Sign(Message::Buffer& buffer) const;
	Security::Verifier::result_type Verify(Message::Buffer const& buffer) const;
	std::uint32_t GetSignatureSize() const;

private:
	Endpoints::EndpointIdType m_endpointIdentifier;
	Endpoints::TechnologyType m_endpointTechnology;

	Security::Encryptor m_encryptor;
	Security::Decryptor m_decryptor;
	Security::Signator m_signator;
	Security::Verifier m_verifier;
	Security::SignatureSizeGetter m_getSignatureSize;

};

//------------------------------------------------------------------------------------------------
