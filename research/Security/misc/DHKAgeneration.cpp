#pragma once
#include <iostream>	// Only needed for printf
#include <cstdlib>	// For rand()
#include <string.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/sha.h> // For SHA1 and SHA2
#include <openssl/evp.h>	// For HMAC
#include <openssl/hmac.h>	
#include <openssl/des.h>
#include <openssl/dh.h>
//USAGE NOTE: gen and genParams() will allocate your pointers for you,
//but you'll have to free them using EVP_PKEY_free(your pointer)
EVP_PKEY* genParams(){
	EVP_PKEY_CTX *pctx;
	EVP_PKEY *params = EVP_PKEY_new();
	pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_DH, NULL);
	EVP_PKEY_paramgen_init(pctx);
	EVP_PKEY_CTX_set_dh_paramgen_prime_len(pctx, 1024);
	EVP_PKEY_paramgen(pctx, &params);
	return params;
}
EVP_PKEY* gen(EVP_PKEY* params){
	EVP_PKEY_CTX *kctx;
	EVP_PKEY *key = EVP_PKEY_new();
	DH *d;
	printf("declared\n");
	printf("paramgened\n");
	kctx = EVP_PKEY_CTX_new(params, NULL);
	EVP_PKEY_CTX_new_id(EVP_PKEY_DH, NULL);
	printf("into keygen\n");
	EVP_PKEY_keygen_init(kctx);
	printf("%d\n", EVP_PKEY_keygen(kctx, &key));
	static BIO* out;
	out = BIO_new_fp(stdout, BIO_NOCLOSE);
	BIO_printf(out, "hi\n");
	printf("opened bio?\n");
	if(key)
		printf("found key\n");
	EVP_PKEY_print_private(out,key,3,NULL);
	//EVP_PKEY_free(params);
	//EVP_PKEY_free(key);
	printf("Hello world\n");
	BIO_free(out);
	return key;
}
