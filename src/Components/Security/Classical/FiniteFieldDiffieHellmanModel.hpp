//----------------------------------------------------------------------------------------------------------------------
// File: FiniteFieldDiffieHellmanModel.hpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include "OpenSSLKeyPair.hpp"
#include "Components/Security/SecurityTypes.hpp"
#include "Components/Security/SecurityDefinitions.hpp"
#include "Components/Security/SynchronizerContext.hpp"
#include "Components/Security/SynchronizerModel.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

namespace Security { class CipherPackage; }

//----------------------------------------------------------------------------------------------------------------------
namespace Security::Classical {
//----------------------------------------------------------------------------------------------------------------------

class FiniteFieldDiffieHellmanModel;

//----------------------------------------------------------------------------------------------------------------------
} // Security::Classical namespace
//----------------------------------------------------------------------------------------------------------------------

class Security::Classical::FiniteFieldDiffieHellmanModel : public Detail::ISynchronizerModel
{
public:
    FiniteFieldDiffieHellmanModel();

    [[nodiscard]] static bool IsKeyAgreementSupported(std::string const& keyAgreement);

    [[nodiscard]] virtual SetupResult SetupKeyExchange(CipherSuite const& cipherSuite) override;
    [[nodiscard]] virtual ComputeFromPublicKeyResult ComputeSharedSecret(PublicKey const& publicKey) override;
    [[nodiscard]] virtual ComputeFromSupplementalDataResult ComputeSharedSecret(SupplementalData const& supplementalData) override;

    // Supplemental data is the data that is appended to the request buffer after generating the session keys. 
    [[nodiscard]] virtual bool HasSupplementalData() const override;
    [[nodiscard]] virtual std::size_t GetSupplementalDataSize() const override;

private:
    std::string m_field;
    OpenSSL::KeyPair m_upKeyPair;
};

//----------------------------------------------------------------------------------------------------------------------
