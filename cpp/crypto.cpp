#include <iostream>	// Only needed for printf
#include <cstdlib>	// For rand()
#include <string.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/sha.h> // For SHA1 and SHA2
#include <openssl/evp.h>	// For HMAC
#include <openssl/hmac.h>	
#include <openssl/des.h>

void triple_des_encrypt(unsigned char*, unsigned char*, unsigned char*, unsigned char*);
void triple_des_decrypt(unsigned char*, unsigned char*, unsigned char*, unsigned char*);
void cast5_encrypt(unsigned char*, unsigned char*, unsigned char*, unsigned char*);
void cast5_decrypt(unsigned char*, unsigned char*, unsigned char*, unsigned char*);
void aes_ctr_encrypt(unsigned char*, unsigned char*, unsigned char*, unsigned char*);
void aes_ctr_decrypt(unsigned char*, unsigned char*, unsigned char*, unsigned char*);
void sha_1(unsigned char*);
void sha_2(unsigned char*);
void hmac_sha2(unsigned char*, unsigned char*);
void hmac_blake2s(unsigned char*, unsigned char*);
void print_output(unsigned char*);
void spacer(void);

int main() {
//	const char plaintext[] = "This is the message.\0";
	unsigned char* plaintext = (unsigned char *)"message";
//	unsigned char* key = "1\0";
	unsigned char* key256 = (unsigned char *)"01234567890123456789012345678901";
//	unsigned char* key256 = "key256\0";
	unsigned char* iv128 = (unsigned char *)"0123456789012345";
//	unsigned char* iv128 = "iv128\0";
	unsigned char ciphertext[512];
	unsigned char decryptedtext[512];

	sha_1(plaintext);
	sha_2(plaintext);
	hmac_sha2(plaintext, key256);
	hmac_blake2s(plaintext, key256);
	spacer();

	aes_ctr_encrypt(plaintext, ciphertext, key256, iv128);
	aes_ctr_decrypt(ciphertext, decryptedtext, key256, iv128);
	spacer();
	cast5_encrypt(plaintext, ciphertext, key256, iv128);
	cast5_decrypt(ciphertext, decryptedtext, key256, iv128);
	spacer();
	triple_des_encrypt(plaintext, ciphertext, key256, iv128);
	triple_des_decrypt(ciphertext, decryptedtext, key256, iv128);

	return 0;
}

void spacer(void){
 printf("\n-----------------------------------------------------\n");
}

/***************CIPHERS*****************/
void triple_des_encrypt(unsigned char* plaintext, unsigned char* ciphertext, unsigned char* k, unsigned char* iv) {
	int length;
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_des_ede3(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, strlen((char *)plaintext));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("3DES Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("3DES Ciphertext (hex representation):\n");
	print_output(ciphertext);
}

void triple_des_decrypt(unsigned char* ciphertext, unsigned char* plaintext, unsigned char* k, unsigned char* iv) {
	int length;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_des_ede3(), NULL, k, iv);
	EVP_DecryptUpdate(ctx, plaintext, &length, ciphertext, strlen((char *)ciphertext));
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, plaintext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("3DES Decrypted text:\n");
	plaintext[plaintext_len] = '\0';
	printf("%s\n\n", plaintext);
}

void cast5_encrypt(unsigned char* plaintext, unsigned char* ciphertext, unsigned char* k, unsigned char* iv) {
	int length;
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_cast5_cbc(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, strlen((char *)plaintext));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("CAST5 Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("CAST5 Ciphertext (hex representation):\n");
	print_output(ciphertext);
}

void cast5_decrypt(unsigned char* ciphertext, unsigned char* plaintext, unsigned char* k, unsigned char* iv) {
	int length;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_cast5_cbc(), NULL, k, iv);
	EVP_DecryptUpdate(ctx, plaintext, &length, ciphertext, strlen((char *)ciphertext));
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, plaintext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("CAST5 Decrypted text:\n");
	plaintext[plaintext_len] = '\0';
	printf("%s\n\n", plaintext);
}

void aes_ctr_encrypt(unsigned char* plaintext, unsigned char* ciphertext, unsigned char* k, unsigned char* iv) {
	int length;
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, plaintext, strlen((char *)plaintext));
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	EVP_CIPHER_CTX_free(ctx);

	printf("AES CTR Initial Plaintext:\n");
	printf("%s\n\n", plaintext);
	printf("AES CTR Ciphertext (hex representation):\n");
	print_output(ciphertext);
}

void aes_ctr_decrypt(unsigned char* ciphertext, unsigned char* plaintext, unsigned char* k, unsigned char* iv) {
	int length;
	int plaintext_len = 0;
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, k, iv);
	EVP_DecryptUpdate(ctx, plaintext, &length, ciphertext, strlen((char *)ciphertext));
	plaintext_len += length;
	EVP_DecryptFinal_ex(ctx, plaintext + length, &length);
	plaintext_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("AES CTR Decrypted text:\n");
	plaintext[plaintext_len] = '\0';
	printf("%s\n\n", plaintext);
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

