//----------------------------------------------------------------------------------------------------------------------
// File: FiniteFielDiffieHellman.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "FiniteFieldDiffieHellmanModel.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Security/CipherPackage.hpp"
#include "Utilities/BufferPrinter.hpp"
#include "Components/Security/SecurityUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/bio.h>
#include <openssl/core_names.h>
#include <openssl/dh.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/param_build.h>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

static std::unordered_map<std::string, std::string> const ParameterMappings {
    { "ffdhe-2048", SN_ffdhe2048 },
    { "ffdhe-3072", SN_ffdhe3072 },
    { "ffdhe-4096", SN_ffdhe4096 },
    { "ffdhe-6144", SN_ffdhe6144 },
    { "ffdhe-8192", SN_ffdhe8192 }
};    

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Security::Classical::FiniteFieldDiffieHellmanModel::KeyEncapsulator {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// } Security::Classical::FiniteFieldDiffieHellmanModel::Context 
//----------------------------------------------------------------------------------------------------------------------

Security::Classical::FiniteFieldDiffieHellmanModel::FiniteFieldDiffieHellmanModel()
    : ISynchronizerModel()
    , m_field()
    , m_upKeyPair()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Classical::FiniteFieldDiffieHellmanModel::IsKeyAgreementSupported(std::string const& keyAgreement)
{
    return local::ParameterMappings.contains(keyAgreement);
}

//----------------------------------------------------------------------------------------------------------------------

Security::Classical::FiniteFieldDiffieHellmanModel::SetupResult Security::Classical::FiniteFieldDiffieHellmanModel::SetupKeyExchange(
    CipherSuite const& cipherSuite)
{
    try {
        auto const& keyAgreement = cipherSuite.GetKeyAgreementName();
        if (auto const itr = local::ParameterMappings.find(keyAgreement); itr != local::ParameterMappings.end()) {
            auto const& [internal, external] = *itr;
            m_field = external;

            std::array<OSSL_PARAM, 2> params = {
                OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>(m_field.c_str()), 0),
                OSSL_PARAM_construct_end()
            };

            auto const upContext = OpenSSL::KeyPairContext{ EVP_PKEY_CTX_new_from_name(nullptr, "DH", nullptr) };
            if (!upContext) {
                return {}; // If we fail to make a context, an error has occurred. 
            }

            if (EVP_PKEY_keygen_init(upContext.get()) <= 0) {
                return {}; // If we fail to initialize key generation context, an error has occurred. 
            }
            
            if (EVP_PKEY_CTX_set_params(upContext.get(), params.data()) <= 0) {
                return {}; // If we fail to set the key generation parameters, an error has occurred. 
            }

            m_upKeyPair = [&upContext] () -> OpenSSL::KeyPair {
                EVP_PKEY* pKeyPair = nullptr;
                if (EVP_PKEY_generate(upContext.get(), &pKeyPair) <= 0) {
                    return nullptr; // If ECDH parameter generation fails, an error has occurred. 
                }

                return OpenSSL::KeyPair{ pKeyPair };
            }();

            if (!m_upKeyPair) {
                return {}; // If we fail to generate the ECDH parameters, an error has occurred. 
            }

            auto publicKey = [&] () -> PublicKey {
                std::size_t size = 0;
                if (EVP_PKEY_get_octet_string_param(m_upKeyPair.get(), OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY, nullptr, 0, &size) <= 0) {
                    return {};
                }

                Buffer buffer(size, 0x00);
                if (EVP_PKEY_get_octet_string_param(m_upKeyPair.get(), OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY, buffer.data(), size, &size) <= 0) {
                    return {};
                }

                return PublicKey{ std::move(buffer) };
            }();

            if (publicKey.IsEmpty()) {
                return {};
            }
            
            return publicKey;
        }

    } catch (...) {
        return {}; // If we have failed to create a key encapsulator for the session, an error has occurred. 
    }

    return {}; // If have failed to setup the model state, an error has occurred. 
}


//----------------------------------------------------------------------------------------------------------------------

Security::Classical::FiniteFieldDiffieHellmanModel::ComputeFromPublicKeyResult Security::Classical::FiniteFieldDiffieHellmanModel::ComputeSharedSecret(
    PublicKey const& publicKey)
{
    if (!m_upKeyPair || m_field.empty()) {
        return {}; // If the key pair has not been setup, an error has occurred. 
    }
    
    // Verify that the provided key is the expected size. 
    {
        std::size_t size = 0;
        if (EVP_PKEY_get_octet_string_param(m_upKeyPair.get(), OSSL_PKEY_PARAM_ENCODED_PUBLIC_KEY, nullptr, 0, &size) <= 0) {
            return {};
        }

        if (publicKey.GetSize() != size) {
            return {};
        }
    }

    auto const upPeerKeyPair = [&] () -> OpenSSL::KeyPair {  
        EVP_PKEY* pKeyPair = EVP_PKEY_new();
        if (!pKeyPair) {
            return {}; // If we fail to new up a key pair, an error has occurred. 
        }

        if (EVP_PKEY_copy_parameters(pKeyPair, m_upKeyPair.get()) <= 0) {
            return {}; // If we fail to copy our parameters to the new key, an error has occurred. 
        }

        if (EVP_PKEY_set1_encoded_public_key(pKeyPair, publicKey.GetData().data(), publicKey.GetSize()) <= 0) {
            return {}; // If we fail to set the key data from the encoded public key, an error has occurred. 
        }

        return OpenSSL::KeyPair{ pKeyPair };
    }();

    if (!upPeerKeyPair) {
        return {}; // If we fail to generate the ECDH parameters, an error has occurred. 
    }

    auto const upDeriveContext = OpenSSL::KeyPairContext{ EVP_PKEY_CTX_new_from_pkey(nullptr, m_upKeyPair.get(), nullptr) };
    if (!upDeriveContext) {
        return {}; // If we fail to make a context, an error has occurred. 
    }

    if (EVP_PKEY_derive_init(upDeriveContext.get()) <= 0) {
        return {}; // If we fail to intialize the context for parsing the peer's public key data, an error has occurred. 
    }

    if (EVP_PKEY_derive_set_peer(upDeriveContext.get(), upPeerKeyPair.get()) <= 0) {
        return {}; // If we fail to intialize the context for parsing the peer's public key data, an error has occurred. 
    }

    std::size_t size = 0;
    if (EVP_PKEY_derive(upDeriveContext.get(), nullptr, &size) <= 0) {
        return {}; // If we fail to intialize the context for parsing the peer's public key data, an error has occurred. 
    }

    Buffer buffer(size, 0x00);
    if (EVP_PKEY_derive(upDeriveContext.get(), buffer.data(), &size) <= 0) {
        return {};
    }

    return std::make_pair(SharedSecret{ std::move(buffer) }, SupplementalData{});
}

//----------------------------------------------------------------------------------------------------------------------

Security::Classical::FiniteFieldDiffieHellmanModel::ComputeFromSupplementalDataResult Security::Classical::FiniteFieldDiffieHellmanModel::ComputeSharedSecret(
    SupplementalData const& supplementalData)
{
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Classical::FiniteFieldDiffieHellmanModel::HasSupplementalData() const { return false; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Classical::FiniteFieldDiffieHellmanModel::GetSupplementalDataSize() const { return 0; }

//----------------------------------------------------------------------------------------------------------------------
