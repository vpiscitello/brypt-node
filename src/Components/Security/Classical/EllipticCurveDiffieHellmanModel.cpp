//----------------------------------------------------------------------------------------------------------------------
// File: EllipticCurveDiffieHellmanModel.cpp
// Description: 
//----------------------------------------------------------------------------------------------------------------------
#include "EllipticCurveDiffieHellmanModel.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include "Components/Security/CipherPackage.hpp"
#include "Utilities/BufferPrinter.hpp"
#include "Components/Security/SecurityUtils.hpp"
//----------------------------------------------------------------------------------------------------------------------
#include <openssl/bio.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
namespace {
namespace local {
//----------------------------------------------------------------------------------------------------------------------

static std::unordered_map<std::string, std::string> const EllipticCurveMappings {
    { "ecdh-b-233", SN_sect233r1 },
    { "ecdh-b-163", SN_sect163r2 },
    { "ecdh-b-283", SN_sect283r1 },
    { "ecdh-b-409", SN_sect409r1 },
    { "ecdh-b-571", SN_sect571r1 },
    { "ecdh-k-163", SN_sect163k1 },
    { "ecdh-k-233", SN_sect233k1 },
    { "ecdh-k-283", SN_sect283k1 },
    { "ecdh-k-409", SN_sect409k1 },
    { "ecdh-k-571", SN_sect571k1 },
    { "ecdh-p-192", SN_X9_62_prime192v1 },
    { "ecdh-p-224", SN_secp224r1 },
    { "ecdh-p-256", SN_X9_62_prime256v1 },
    { "ecdh-p-384", SN_secp384r1 },
    { "ecdh-p-521", SN_secp521r1 },
    { "ecdh-secp-112-r1", SN_secp112r1 },
    { "ecdh-secp-112-r2", SN_secp112r2 },
    { "ecdh-secp-128-r1", SN_secp128r1 },
    { "ecdh-secp-128-r2", SN_secp128r2 },
    { "ecdh-secp-160-k1", SN_secp160k1 },
    { "ecdh-secp-160-r1", SN_secp160r1 },
    { "ecdh-secp-160-r2", SN_secp160r2 },
    { "ecdh-secp-192-k1", SN_secp192k1 },
    { "ecdh-secp-224-k1", SN_secp224k1 },
    { "ecdh-secp-224-r1", SN_secp224r1 },
    { "ecdh-secp-256-k1", SN_secp256k1 },
    { "ecdh-secp-384-r1", SN_secp384r1 },
    { "ecdh-secp-521-r1", SN_secp521r1 },
    { "ecdh-prime-192-v1", SN_X9_62_prime192v1 },
    { "ecdh-prime-192-v2", SN_X9_62_prime192v2 },
    { "ecdh-prime-192-v3", SN_X9_62_prime192v3 },
    { "ecdh-prime-239-v1", SN_X9_62_prime239v1 },
    { "ecdh-prime-239-v2", SN_X9_62_prime239v2 },
    { "ecdh-prime-239-v3", SN_X9_62_prime239v3 },
    { "ecdh-prime-256-v1", SN_X9_62_prime256v1 },
    { "ecdh-sect-113-r1", SN_sect113r1 },
    { "ecdh-sect-113-r2", SN_sect113r2 },
    { "ecdh-sect-131-r1", SN_sect131r1 },
    { "ecdh-sect-131-r2", SN_sect131r2 },
    { "ecdh-sect-163-k1", SN_sect163k1 },
    { "ecdh-sect-163-r1", SN_sect163r1 },
    { "ecdh-sect-163-r2", SN_sect163r2 },
    { "ecdh-sect-193-r1", SN_sect193r1 },
    { "ecdh-sect-193-r2", SN_sect193r2 },
    { "ecdh-sect-233-k1", SN_sect233k1 },
    { "ecdh-sect-233-r1", SN_sect233r1 },
    { "ecdh-sect-239-k1", SN_sect239k1 },
    { "ecdh-sect-283-k1", SN_sect283k1 },
    { "ecdh-sect-283-r1", SN_sect283r1 },
    { "ecdh-sect-409-k1", SN_sect409k1 },
    { "ecdh-sect-409-r1", SN_sect409r1 },
    { "ecdh-sect-571-k1", SN_sect571k1 },
    { "ecdh-sect-571-r1", SN_sect571r1 },
    { "ecdh-c2-pnb-163-v1", SN_X9_62_c2pnb163v1 },
    { "ecdh-c2-pnb-163-v2", SN_X9_62_c2pnb163v2 },
    { "ecdh-c2-pnb-163-v3", SN_X9_62_c2pnb163v3 },
    { "ecdh-c2-pnb-176-v1", SN_X9_62_c2pnb176v1 },
    { "ecdh-c2-tnb-191-v1", SN_X9_62_c2tnb191v1 },
    { "ecdh-c2-tnb-191-v2", SN_X9_62_c2tnb191v2 },
    { "ecdh-c2-tnb-191-v3", SN_X9_62_c2tnb191v3 },
    { "ecdh-c2-pnb-208-w1", SN_X9_62_c2pnb208w1 },
    { "ecdh-c2-tnb-239-v1", SN_X9_62_c2tnb239v1 },
    { "ecdh-c2-tnb-239-v2", SN_X9_62_c2tnb239v2 },
    { "ecdh-c2-tnb-239-v3", SN_X9_62_c2tnb239v3 },
    { "ecdh-c2-pnb-272-w1", SN_X9_62_c2pnb272w1 },
    { "ecdh-c2-pnb-304-w1", SN_X9_62_c2pnb304w1 },
    { "ecdh-c2-tnb-359-v1", SN_X9_62_c2tnb359v1 },
    { "ecdh-c2-pnb-368-w1", SN_X9_62_c2pnb368w1 },
    { "ecdh-c2-tnb-431-r1", SN_X9_62_c2tnb431r1 },
    { "ecdh-wap-wsg-idm-ecid-wtls-1", SN_wap_wsg_idm_ecid_wtls1 },
    { "ecdh-wap-wsg-idm-ecid-wtls-3", SN_wap_wsg_idm_ecid_wtls3 },
    { "ecdh-wap-wsg-idm-ecid-wtls-4", SN_wap_wsg_idm_ecid_wtls4 },
    { "ecdh-wap-wsg-idm-ecid-wtls-5", SN_wap_wsg_idm_ecid_wtls5 },
    { "ecdh-wap-wsg-idm-ecid-wtls-6", SN_wap_wsg_idm_ecid_wtls6 },
    { "ecdh-wap-wsg-idm-ecid-wtls-7", SN_wap_wsg_idm_ecid_wtls7 },
    { "ecdh-wap-wsg-idm-ecid-wtls-8", SN_wap_wsg_idm_ecid_wtls8 },
    { "ecdh-wap-wsg-idm-ecid-wtls-9", SN_wap_wsg_idm_ecid_wtls9 },
    { "ecdh-wap-wsg-idm-ecid-wtls-10", SN_wap_wsg_idm_ecid_wtls10 },
    { "ecdh-wap-wsg-idm-ecid-wtls-11", SN_wap_wsg_idm_ecid_wtls11 },
    { "ecdh-wap-wsg-idm-ecid-wtls-12", SN_wap_wsg_idm_ecid_wtls12 },
    { "ecdh-oakley-ec2n-3", SN_ipsec3 },
    { "ecdh-oakley-ec2n-4", SN_ipsec4 },
    { "ecdh-brainpool-p-160-r1", SN_brainpoolP160r1 },
    { "ecdh-brainpool-p-160-t1", SN_brainpoolP160t1 },
    { "ecdh-brainpool-p-192-r1", SN_brainpoolP192r1 },
    { "ecdh-brainpool-p-192-t1", SN_brainpoolP192t1 },
    { "ecdh-brainpool-p-224-r1", SN_brainpoolP224r1 },
    { "ecdh-brainpool-p-224-t1", SN_brainpoolP224t1 },
    { "ecdh-brainpool-p-256-r1", SN_brainpoolP256r1 },
    { "ecdh-brainpool-p-256-t1", SN_brainpoolP256t1 },
    { "ecdh-brainpool-p-320-r1", SN_brainpoolP320r1 },
    { "ecdh-brainpool-p-320-t1", SN_brainpoolP320t1 },
    { "ecdh-brainpool-p-384-r1", SN_brainpoolP384r1 },
    { "ecdh-brainpool-p-384-t1", SN_brainpoolP384t1 },
    { "ecdh-brainpool-p-512-r1", SN_brainpoolP512r1 },
    { "ecdh-brainpool-p-512-t1", SN_brainpoolP512t1 },
};    

//----------------------------------------------------------------------------------------------------------------------
} // local namespace
} // namespace
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Security::Classical::EllipticCurveDiffieHellmanModel::KeyEncapsulator {
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// } Security::Classical::EllipticCurveDiffieHellmanModel::Context 
//----------------------------------------------------------------------------------------------------------------------

Security::Classical::EllipticCurveDiffieHellmanModel::EllipticCurveDiffieHellmanModel()
    : ISynchronizerModel()
    , m_curve()
    , m_upKeyPair()
{
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Classical::EllipticCurveDiffieHellmanModel::IsKeyAgreementSupported(std::string const& keyAgreement)
{
    return local::EllipticCurveMappings.contains(keyAgreement);
}

//----------------------------------------------------------------------------------------------------------------------

Security::Classical::EllipticCurveDiffieHellmanModel::SetupResult Security::Classical::EllipticCurveDiffieHellmanModel::SetupKeyExchange(
    CipherSuite const& cipherSuite)
{
    try {
        auto const& keyAgreement = cipherSuite.GetKeyAgreementName();
        if (auto const itr = local::EllipticCurveMappings.find(keyAgreement); itr != local::EllipticCurveMappings.end()) {
            auto const& [internal, external] = *itr;

            m_curve = external;

            std::array<OSSL_PARAM, 2> params = {
                OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>(m_curve.c_str()), 0),
                OSSL_PARAM_construct_end()
            };

            auto const upContext = OpenSSL::KeyPairContext{ EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr) };
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
                if (EVP_PKEY_get_octet_string_param(m_upKeyPair.get(), OSSL_PKEY_PARAM_PUB_KEY, nullptr, 0, &size) <= 0) {
                    return {};
                }

                Buffer buffer(size, 0x00);
                if (EVP_PKEY_get_octet_string_param(m_upKeyPair.get(), OSSL_PKEY_PARAM_PUB_KEY, buffer.data(), size, &size) <= 0) {
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

Security::Classical::EllipticCurveDiffieHellmanModel::ComputeFromPublicKeyResult Security::Classical::EllipticCurveDiffieHellmanModel::ComputeSharedSecret(
    PublicKey const& publicKey)
{
    if (!m_upKeyPair || m_curve.empty()) {
        return {}; // If the key pair has not been setup, an error has occurred. 
    }

    auto const upPeerKeyPair = [&] () -> OpenSSL::KeyPair {
        EVP_PKEY* pKeyPair = nullptr;

        auto const upPeerContext = OpenSSL::KeyPairContext{ EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr) };
        if (!upPeerContext) {
            return {}; // If we fail to make a context, an error has occurred. 
        }

        if (EVP_PKEY_fromdata_init(upPeerContext.get()) <= 0) {
            return {}; // If we fail to intialize the context for parsing the peer's public key data, an error has occurred. 
        }

        std::array<OSSL_PARAM, 3> params = {
            OSSL_PARAM_construct_utf8_string(OSSL_PKEY_PARAM_GROUP_NAME, const_cast<char*>(m_curve.data()), 0),
            OSSL_PARAM_construct_octet_string(OSSL_PKEY_PARAM_PUB_KEY, const_cast<std::uint8_t*>(publicKey.GetData().data()), publicKey.GetSize()),
            OSSL_PARAM_construct_end()
        };

        if (EVP_PKEY_fromdata(upPeerContext.get(), &pKeyPair, EVP_PKEY_PUBLIC_KEY, params.data()) <= 0) {
            return nullptr; // If ECDH parameter generation fails, an error has occurred. 
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

Security::Classical::EllipticCurveDiffieHellmanModel::ComputeFromSupplementalDataResult Security::Classical::EllipticCurveDiffieHellmanModel::ComputeSharedSecret(
    SupplementalData const& supplementalData)
{
    return {};
}

//----------------------------------------------------------------------------------------------------------------------

bool Security::Classical::EllipticCurveDiffieHellmanModel::HasSupplementalData() const { return false; }

//----------------------------------------------------------------------------------------------------------------------

std::size_t Security::Classical::EllipticCurveDiffieHellmanModel::GetSupplementalDataSize() const { return 0; }

//----------------------------------------------------------------------------------------------------------------------
