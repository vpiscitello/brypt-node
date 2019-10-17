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
#include <openssl/ec.h>
//USAGE NOTE: gen and genParams() will allocate your pointers for you,
//but you'll have to free them using EVP_PKEY_free(your pointer)
EVP_PKEY* genECDHParams(){
	EVP_PKEY_CTX *pctx;
	EVP_PKEY *params = EVP_PKEY_new();
	pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
	EVP_PKEY_paramgen_init(pctx);
	EVP_PKEY_CTX_set_ec_paramgen_curve_nid(pctx, NID_X9_62_prime256v1);
	EVP_PKEY_paramgen(pctx, &params);
	return params;
}
EVP_PKEY* genECDH(EVP_PKEY* params){
	EVP_PKEY_CTX *kctx;
	EVP_PKEY *key = EVP_PKEY_new();
	DH *d;
	kctx = EVP_PKEY_CTX_new(params, NULL);
	EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
	EVP_PKEY_keygen_init(kctx);
	printf("keygen succ: %d\n", EVP_PKEY_keygen(kctx, &key));
	static BIO* out;
	out = BIO_new_fp(stdout, BIO_NOCLOSE);
	if(key)
		printf("found key\n");
	EVP_PKEY_print_private(out,key,3,NULL);
	//EVP_PKEY_free(params);
	//EVP_PKEY_free(key);
	BIO_free(out);
	return key;
}
/*
int main(){
	EVP_PKEY* params = genECDHParams();
	EVP_PKEY* comp = genECDH(params);
	return 0;
}
*/
