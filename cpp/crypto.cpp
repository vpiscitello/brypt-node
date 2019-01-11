#include <iostream>	// Only needed for printf
#include <cstdlib>	// For rand()
#include <string.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/sha.h> // For SHA1 and SHA2
#include <openssl/evp.h>	// For HMAC
#include <openssl/hmac.h>	
#include <openssl/des.h>

void triple_des(unsigned char* input, unsigned char* k, unsigned char* iv);
void cast5(unsigned char* input, unsigned char* k, unsigned char* iv);
void aes_ctr(unsigned char* input, unsigned char* k, unsigned char* iv);
void sha_1(unsigned char*);
void sha_2(unsigned char*);
void hmac_sha2(unsigned char*, unsigned char*);
void hmac_blake2s(unsigned char*, unsigned char*);
void print_output(unsigned char*);


int main() {
//	const char mssg[] = "This is the message.\0";
	unsigned char* mssg = (unsigned char *)"message";
//	unsigned char* key = "1\0";
	unsigned char* key256 = (unsigned char *)"01234567890123456789012345678901";
//	unsigned char* key256 = "key256\0";
	unsigned char* iv128 = (unsigned char *)"0123456789012345";
//	unsigned char* iv128 = "iv128\0";

	sha_1(mssg);
	sha_2(mssg);
	hmac_sha2(mssg, key256);
	hmac_blake2s(mssg, key256);

	aes_ctr(mssg, key256, iv128);
	cast5(mssg, key256, iv128);
	triple_des(mssg, key256, iv128);

	return 0;
}

/***************CIPHERS*****************/
void triple_des(unsigned char* input, unsigned char* k, unsigned char* iv) {
	int length;
	unsigned char ciphertext[512];
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_des_ede3(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, input, strlen((char *)input));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("3DES Plaintext:\n");
	printf("%s\n\n", input);
	printf("3DES Ciphertext (hex representation):\n");
//	print_output(ciphertext);
}

void cast5(unsigned char* input, unsigned char* k, unsigned char* iv) {
	int length;
	unsigned char ciphertext[512];
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_cast5_cbc(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, input, strlen((char *)input));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("CAST5 Plaintext:\n");
	printf("%s\n\n", input);
	printf("CAST5 Ciphertext (hex representation):\n");
//	print_output(ciphertext);
}

void aes_ctr(unsigned char* input, unsigned char* k, unsigned char* iv) {
	int length;
	unsigned char ciphertext[512];
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, input, strlen((char *)input));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("AES-CTR Plaintext:\n");
	printf("%s\n\n", input);
	printf("AES-CTR Ciphertext (hex representation):\n");
	//print_output(ciphertext);
}

/***************HASHES******************/
void sha_1(unsigned char* input){
	unsigned char output[512];
	SHA1(input, strlen((char *)input), output);
	printf("SHA1: \n");
	print_output(output);
}

void sha_2(unsigned char* input){
	unsigned char output[512];
	SHA256(input, strlen((char *)input), output);
	printf("SHA2: \n");
	print_output(output);
}

void hmac_sha2(unsigned char* input, unsigned char* key){
	unsigned char* digest;
	digest = HMAC(EVP_sha256(), key, strlen((char *)key), input, strlen((char *)input), NULL, NULL);
	printf("HMAC_SHA2: \n");
	print_output(digest);
}

void hmac_blake2s(unsigned char* input, unsigned char* key){
	unsigned char* digest;
	digest = HMAC(EVP_blake2s256(), key, strlen((char *)key), input, strlen((char *)input), NULL, NULL);
	printf("HMAC_BLAKE2s256: \n");
	print_output(digest);
}

void print_output(unsigned char* output) {
	for(int i = 0; i < strlen((char *)output); i++) {
		printf("%02x", output[i]);
	}
	printf("\n\n");
}

