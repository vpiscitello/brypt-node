//----------------------------------------------------------------------------------------------------------------------
// File: OpenSSLPublicKey.hpp
//----------------------------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------------------------
#include <memory>
//----------------------------------------------------------------------------------------------------------------------

typedef struct evp_pkey_ctx_st EVP_PKEY_CTX;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct ossl_param_st OSSL_PARAM;
typedef struct ossl_param_bld_st OSSL_PARAM_BLD;

extern "C" void EVP_PKEY_CTX_free(EVP_PKEY_CTX* context);
extern "C" void EVP_PKEY_free(EVP_PKEY* key);
extern "C" void OSSL_PARAM_free(OSSL_PARAM *p);
extern "C" void OSSL_PARAM_BLD_free(OSSL_PARAM_BLD* bld);

//----------------------------------------------------------------------------------------------------------------------
namespace Security::OpenSSL {
//----------------------------------------------------------------------------------------------------------------------

struct KeyPairContextDeleter
{
    void operator()(EVP_PKEY_CTX* pContext) const
    {
        EVP_PKEY_CTX_free(pContext);
    }
};

using KeyPairContext = std::unique_ptr<EVP_PKEY_CTX, KeyPairContextDeleter>;

struct KeyPairDeleter
{
    void operator()(EVP_PKEY* pKey) const
    {
        EVP_PKEY_free(pKey);
    }
};

using KeyPair = std::unique_ptr<EVP_PKEY, KeyPairDeleter>;

//----------------------------------------------------------------------------------------------------------------------
} // Security namespace
//----------------------------------------------------------------------------------------------------------------------
