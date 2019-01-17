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
	plaintext = (unsigned char *)"message";
	key = (unsigned char *)"01234567890123456789012345678901";
	iv = (unsigned char *)"0123456789012345";
}

crypto::~crypto(){}

void crypto::clear_ciphertext(){
	int i;

	for(i=0; i < strlen((char *)ciphertext); i++) {
		ciphertext[i] = '\0';
	}
}

void crypto::clear_decryptedtext(){
	int i;

	for(i=0; i < strlen((char *)decryptedtext); i++) {
		decryptedtext[i] = '\0';
	}
}

void crypto::clear_plaintext(){
	int i;

	for(i=0; i < strlen((char *)plaintext); i++) {
		plaintext[i] = '\0';
	}
}

void crypto::set_plaintext(unsigned char *p){
	strncpy((char *)plaintext, (char *)p, strlen((char *)p));
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
	int length;
	EVP_CIPHER_CTX *ctx;

	clear_ciphertext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_des_ede3(), NULL, key, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, strlen((char *)plaintext));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("3DES Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("3DES Ciphertext (hex representation):\n");
	print_output(ciphertext);
}
/*
crypto::triple_des_decrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* key;
unsigned char* iv;
unsigned char ciphertext[BUFF_SIZE];
*/
void crypto::triple_des_decrypt() {
	int length;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	clear_decryptedtext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_des_ede3(), NULL, key, iv);
	EVP_DecryptUpdate(ctx, decryptedtext, &length, ciphertext, strlen((char *)ciphertext));
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, decryptedtext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("3DES Decrypted text:\n");
	decryptedtext[plaintext_len] = '\0';
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
	int length;
	EVP_CIPHER_CTX *ctx;

	clear_ciphertext();
	
	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_cast5_cbc(), NULL, key, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, strlen((char *)plaintext));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("CAST5 Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("CAST5 Ciphertext (hex representation):\n");
	print_output(ciphertext);
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
	EVP_DecryptUpdate(ctx, decryptedtext, &length, ciphertext, strlen((char *)ciphertext));
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, decryptedtext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("CAST5 Decrypted text:\n");
	decryptedtext[plaintext_len] = '\0';
	printf("%s\n\n", decryptedtext);
}
/*
crypto::aes_ctr_encrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* plaintext;
unsigned char* key;
unsigned char* iv;
*/
void crypto::aes_ctr_encrypt() {
	int length;
	EVP_CIPHER_CTX *ctx;

	clear_ciphertext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, strlen((char *)plaintext));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("AES CTR Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("AES CTR Ciphertext (hex representation):\n");
	print_output(ciphertext);
}
/*
crypto::aes_ctr_decrypt()
REQUIRED MEMBER VARIABLES:
unsigned char* key;
unsigned char* iv;
unsigned char ciphertext[BUFF_SIZE];
*/
void crypto::aes_ctr_decrypt() {
	int length;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	clear_decryptedtext();

	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, key, iv);
	EVP_DecryptUpdate(ctx, decryptedtext, &length, ciphertext, strlen((char *)ciphertext));
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
	print_output(hash);
}

void crypto::sha_2(unsigned char* input){
	SHA256(input, strlen((char *)input), hash);
	printf("SHA2: \n");
	print_output(hash);
}

void crypto::hmac_sha2(unsigned char* input){
	digest = HMAC(EVP_sha256(), key, strlen((char *)key), input, strlen((char *)input), NULL, NULL);
	printf("HMAC_SHA2: \n");
	print_output(digest);
}

void crypto::hmac_blake2s(unsigned char* input){
	digest = HMAC(EVP_blake2s256(), key, strlen((char *)key), input, strlen((char *)input), NULL, NULL);
	printf("HMAC_BLAKE2s256: \n");
	print_output(digest);
}

void crypto::print_output(unsigned char* output) {
	for(int i = 0; i < strlen((char *)output); i++) {
		printf("%02x", output[i]);
	}
	printf("\n\n");
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
