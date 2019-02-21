/*#include <iostream>	// Only needed for printf
#include <cstdlib>	// For rand()
#include <string.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/sha.h> // For SHA1 and SHA2
#include <openssl/evp.h>	// For HMAC
#include <openssl/hmac.h>	
#include <openssl/des.h>
*/

#include "./crypto.h"

crypto::crypto() {
	memset(plaintext, 0x00, BUFF_SIZE);
	set_plaintext((unsigned char *)"The quick brown fox jumps over the lazy dog");
	set_our_key((unsigned char *)"01234567890123456789012345678901",32);
	iv = (unsigned char *)"0123456789012345";
}

crypto::~crypto(){}

void crypto::clear_hash(){
	int i;

	for(i=0; i < HASH_SIZE; i++) {
		hash[i] = '\0';
	}
}

void crypto::clear_ciphertext(){
	int i;

	for(i=0; i < BUFF_SIZE; i++) {
		ciphertext[i] = '\0';
	}
	ctxt_len = 0;		// Clear the ciphertext length variable
}

void crypto::clear_decryptedtext(){
	int i;

	for(i=0; i < BUFF_SIZE; i++) {
		decryptedtext[i] = '\0';
	}
}

void crypto::clear_plaintext(){
	int i;

	for(i=0; i < BUFF_SIZE; i++) {
		plaintext[i] = '\0';
	}
}

void crypto::set_plaintext(unsigned char *p){
	memset(plaintext, 0x00, BUFF_SIZE);
	strncpy((char *)plaintext, (char *)p, strlen((char *)p));
	ptxt_len = strlen((const char *)plaintext);
//	ptxt_len = 16*(ptxt_len/16)+16; //cast to 16-byte blocks
}
void crypto::set_our_key(unsigned char *k, int size){
	if(size%8!=0 || size>OUR_KEY_SIZE){
		printf("WEEWOOWEEWOO THAT KEY SEEMS SKETCHY!\n");
	}
	memset(key, 0x00, OUR_KEY_SIZE);
	strncpy((char*) key, (char *)k, size);
}
/***************CIPHERS*****************/
/*
Object type example:
Crypto crypto;
crypto.set_plaintext((unsigned char *)"hello");
crypto.set_key((unsigned char *)"very_secure");
crypto.set_iv((unsigned char *)"nonce");
crypto.triple_des_encrypt();
printf(crypto.get_ciphertext());
crypto.triple_des_decrypt();
printf(crypto.get_plaintext());
*/
/*
crypto::triple_des_encrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* plaintext;
unsigned char* key;
unsigned char* iv;
*/
void crypto::triple_des_encrypt() {
	int length = 0;
	EVP_CIPHER_CTX *ctx;

	clear_ciphertext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_des_ede3(), NULL, key, iv);
//	EVP_CIPHER_CTX_set_padding(ctx, 0);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, ptxt_len + 1);
	ctxt_len = length;
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	ctxt_len += length;
	EVP_CIPHER_CTX_free(ctx);
	printf("3DES Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("3DES Ciphertext (hex representation):\n");
	print_output(ciphertext, ctxt_len);
}
/*
crypto::triple_des_decrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* key;
unsigned char* iv;
unsigned char ciphertext[BUFF_SIZE];
*/
void crypto::triple_des_decrypt() {
	int length = 0;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	clear_decryptedtext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_des_ede3(), NULL, key, iv);
	EVP_DecryptUpdate(ctx, decryptedtext, &length, ciphertext, ctxt_len);
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, decryptedtext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("3DES Decrypted text:\n");
	decryptedtext[plaintext_len] = '\0';
	ptxt_len = plaintext_len;
	printf("%s\n\n", decryptedtext);
}
/*
crypto::cast5_encrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* plaintext;
unsigned char* key;
unsigned char* iv;
*/
void crypto::cast5_encrypt() {
	int length = 0;
	EVP_CIPHER_CTX *ctx;

	clear_ciphertext();
	
	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_cast5_cbc(), NULL, key, iv);
	EVP_CIPHER_CTX_set_padding(ctx, 0);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, ptxt_len + 1);
	ctxt_len = length;
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	ctxt_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("CAST5 Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("CAST5 Ciphertext (hex representation):\n");
	print_output(ciphertext, ctxt_len);
}
/*
crypto::cast5_decrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* key;
unsigned char* iv;
unsigned char ciphertext[BUFF_SIZE];
*/
void crypto::cast5_decrypt() {
	int length;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	clear_decryptedtext();
	
	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_cast5_cbc(), NULL, key, iv);
	EVP_DecryptUpdate(ctx, decryptedtext, &length, ciphertext, ctxt_len);
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, decryptedtext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("CAST5 Decrypted text:\n");
	decryptedtext[plaintext_len] = '\0';
	ctxt_len = length;
	printf("%s\n\n", decryptedtext);
}
/*
crypto::aes_ctr_encrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* plaintext;
unsigned char* key;
unsigned char* iv;
*/
void crypto::aes_ctr_256_encrypt() {
	int length = 0;
	EVP_CIPHER_CTX *ctx;

	clear_ciphertext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, ptxt_len + 1);
	ctxt_len = length;
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	ctxt_len += length;
	EVP_CIPHER_CTX_free(ctx);


	printf("AES CTR 256 Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("AES CTR 256 Ciphertext (hex representation):\n");
	print_output(ciphertext, ctxt_len);
}
/*
crypto::aes_ctr_decrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* key;
unsigned char* iv;
unsigned char ciphertext[BUFF_SIZE];
*/
void crypto::aes_ctr_256_decrypt() {
	int length;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	clear_decryptedtext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv);
	EVP_DecryptUpdate(ctx, decryptedtext, &length, ciphertext, ctxt_len);
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, decryptedtext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("AES CTR 256 Decrypted text:\n");
	decryptedtext[plaintext_len] = '\0';
	printf("%s\n\n", decryptedtext);
}

void crypto::aes_ctr_128_encrypt() {
	int length = 0;
	EVP_CIPHER_CTX *ctx;

	clear_ciphertext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, key, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, ptxt_len + 1);
	ctxt_len = length;
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	ctxt_len += length;
	EVP_CIPHER_CTX_free(ctx);


	printf("AES CTR 128 Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("AES CTR 128 Ciphertext (hex representation):\n");
	print_output(ciphertext, ctxt_len);
}


void crypto::aes_ctr_128_decrypt() {
	int length;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	clear_decryptedtext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, key, iv);
	EVP_DecryptUpdate(ctx, decryptedtext, &length, ciphertext, ctxt_len);
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, decryptedtext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("AES CTR Decrypted text:\n");
	decryptedtext[plaintext_len] = '\0';
	printf("%s\n\n", decryptedtext);
}


/***************HASHES******************/
void crypto::sha_1(unsigned char* input){
	SHA1(input, strlen((char *)input), hash);
	printf("SHA1: \n");
	print_output(hash, HASH_SIZE);
}

void crypto::sha_2(unsigned char* input){
	int i;
	SHA256(input, strlen((char *)input), hash);
	for(i = 0; i<512; i++){
		printf("%d", input[i]);
	}
	printf("SHA2: \n");
	print_output(hash, HASH_SIZE);
}

void crypto::hmac_sha2(unsigned char* input){
	unsigned int length = 0;
	digest = HMAC(EVP_sha256(), key, OUR_KEY_SIZE, input, strlen((char *)input), NULL, &length);
	printf("HMAC_SHA2: \n");
	print_output(digest, (int)length);
}
/*
void crypto::hmac_blake2s(unsigned char* input){
	unsigned int length = 0;
	digest = HMAC(EVP_blake2s256(), key, OUR_KEY_SIZE, input, strlen((char *)input), NULL, &length);
	printf("HMAC_BLAKE2s256: \n");
	print_output(digest, (int)length);
}
*/
/* INTELLECTUAL PROPERTY NOTE: heavily influenced by wiki.openssl.org/index.php/Diffie_Hellman*/
void crypto::modded_DHKA(){
	DH *privkey;
	int codes;
	int secret_size;
/* Generate the parameters to be used */
	if(NULL == (privkey = DH_new())) handleErrors();
	if(1 != DH_generate_parameters_ex(privkey, 1024, DH_GENERATOR_2, NULL)) handleErrors();
	
	if(1 != DH_check(privkey, &codes)) handleErrors();
	if(codes != 0){
    /* Problems have been found with the generated parameters */
    /* Handle these here - we'll just abort for this example */
	    printf("DH_check failed\n");
	    abort();
	}

/* Generate the public and private key pair */
	if(1 != DH_generate_key(privkey)) handleErrors();

/* Send the public key to the peer.
 *  * How this occurs will be specific to your situation (see main text below) */
	//BIO_dump_fp(stdout, (const char *)privkey, 128);
	DHparams_print(garbage, garbage);
	DHparams_print_fd(stdout, privkey);
	printf("dhparams failed\n");

/* Receive the public key from the peer. In this example we're just hard coding a value */
	BIGNUM *pubkey = NULL;
	if(0 == (BN_dec2bn(&pubkey, "01234567890123456789012345678901234567890123456789"))) handleErrors();

/* Compute the shared secret */
	unsigned char *secret;
	if(NULL == (secret = (unsigned char *)OPENSSL_malloc(sizeof(unsigned char) * (DH_size(privkey))))) handleErrors();
	
	if(0 > (secret_size = DH_compute_key(secret, pubkey, privkey))) handleErrors();

	/* Do something with the shared secret */
	/* Note secret_size may be less than DH_size(privkey) */
	//printf("The shared secret is:\n%s\n", secret);
	BIO_dump_fp(stdout, (const char *)secret, secret_size);
	printf("secret size was %d\n", secret_size);
	OPENSSL_free(secret);
	DH_free(privkey);
}
void crypto::handleErrors(){
	printf("HALT AND CATCH FIIIIIIIIIIRE\n");
	abort();
}
void crypto::print_output(unsigned char* output, int len) {
	for(int i = 0; i < len; i++) {
		printf("%02x", output[i]);
	}
	printf("\n\n");
	printf("CTXT len: %d\n", ctxt_len);
}

/*****************GETTERS****************/

unsigned char* crypto::get_plaintext(){
	return plaintext;
}

unsigned char* crypto::get_ciphertext(){
	return ciphertext;
}

unsigned char* crypto::get_decryptedtext(){
	return decryptedtext;
}

unsigned char* crypto::get_hash(){
	return hash;
}

unsigned char* crypto::get_digest() {
	return digest;
}
