#include <stdio.h>	// Only needed for printf
#include <stdlib.h>	// For rand()
#include <string.h>
#include <openssl/sha.h> // For SHA1 and SHA2
#include <openssl/evp.h>	// For HMAC
#include <openssl/hmac.h>	
#include <openssl/des.h>

#define MAX_CTXT 1024
#define HASH_SIZE 32

void triple_des(unsigned char* input, unsigned char* k, unsigned char* iv,
int mssg_len);
void aes256_ctr(unsigned char* input, unsigned char* k, unsigned char* iv,
int mssg_len);
void aes128_ctr(unsigned char* input, unsigned char* k, unsigned char* iv,
int mssg_len);
void sha_1(unsigned char*);
void sha_2(unsigned char*);
void hmac_sha2(unsigned char*, unsigned char*);
void hmac_blake2s(unsigned char*, unsigned char*);
void print_output(unsigned char*, int);

/***************CIPHERS*****************/
void triple_des(unsigned char* input, unsigned char* k, unsigned char* iv, 
int mssg_len) {
	int length;
	unsigned char padded_ptxt[MAX_CTXT];
	int output_len = 0;
	unsigned char ciphertext[MAX_CTXT];
	memset(ciphertext, 0x00, sizeof(ciphertext));
	memset(padded_ptxt, 0x00, sizeof(padded_ptxt));	
	strncpy(padded_ptxt, input, strlen(input));
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_des_ede3(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, input, mssg_len + 1);
	output_len += length;
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	output_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("3DES Plaintext:\n");
	printf("%s\n\n", padded_ptxt);
	printf("3DES Ciphertext (hex representation):\n");
	print_output(ciphertext, output_len);
}

void cast5(unsigned char* input, unsigned char* k, unsigned char* iv, 
int mssg_len) {
	int length;
	int output_len = 0;
	unsigned char ciphertext[MAX_CTXT];
	memset(ciphertext, 0x00, sizeof(ciphertext));
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_cast5_cbc(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, input, mssg_len + 1);
	output_len += length;
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	output_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("CAST5 Plaintext:\n");
	printf("%s\n\n", input);
	printf("CAST5 Ciphertext (hex representation):\n");
	print_output(ciphertext, output_len);
}

void aes256_ctr(unsigned char* input, unsigned char* k, unsigned char* iv, 
int mssg_len) {
	int length;
	unsigned char padded_ptxt[MAX_CTXT];
	int output_len = 0;
	unsigned char ciphertext[MAX_CTXT];
	memset(ciphertext, 0x00, sizeof(ciphertext));
	memset(padded_ptxt, 0x00, sizeof(padded_ptxt));	
	strncpy(padded_ptxt, input, strlen(input));
	EVP_CIPHER_CTX *ctx;
	mssg_len = 16*(mssg_len/16) + 16;
	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), NULL, k, iv);
	printf("mssg_len %d\n",mssg_len);
	EVP_EncryptUpdate(ctx, ciphertext, &length, padded_ptxt, mssg_len);
	output_len += length;
	printf("flushing\n");
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	output_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("AES-CTR-256 Plaintext:\n");
	printf("%s\n\n", padded_ptxt);
	printf("Output len: %d AES-CTR-256 Ciphertext (hex representation):\n", output_len);
	print_output(ciphertext, output_len);
}

void aes128_ctr(unsigned char* input, unsigned char* k, unsigned char* iv, 
int mssg_len) {
	int length;
	int output_len = 0;
	unsigned char ciphertext[MAX_CTXT];
	memset(ciphertext, 0x00, sizeof(ciphertext));
	EVP_CIPHER_CTX *ctx;

	ctx = EVP_CIPHER_CTX_new();

	EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, k, iv);
	EVP_EncryptUpdate(ctx, ciphertext, &length, input, mssg_len + 1);
	output_len += length;
	EVP_EncryptFinal_ex(ctx, ciphertext + length, &length);
	output_len += length;
	EVP_CIPHER_CTX_free(ctx);

	printf("AES-CTR-128 Plaintext:\n");
	printf("%s\n\n", input);
	printf("AES-CTR-128 Ciphertext (hex representation):\n");
	print_output(ciphertext, output_len);
}

/***************HASHES******************/
void sha_1(unsigned char* input){
	unsigned char output[30];
	memset(output, 0x00, sizeof(output));
	SHA1(input, strlen(input), output);
	printf("SHA1: \n");
	print_output(output, HASH_SIZE);
}

void sha_2(unsigned char* input){
	unsigned char output[30];
	memset(output, 0x00, sizeof(output));
	SHA256(input, strlen(input), output);
	printf("SHA2: \n");
	print_output(output, HASH_SIZE);
}
/*
void hmac_sha2(unsigned char* input, unsigned char* key){
	unsigned char* digest;
	digest = HMAC(EVP_sha256(), key, strlen(key), input, strlen(input), NULL, NULL);
	printf("HMAC_SHA2: \n");
	print_output(digest, HASH_SIZE);
}

void hmac_blake2s(unsigned char* input, unsigned char* key){
	unsigned char* digest;
	//replacing blake2s256() w/blake2s
	digest = HMAC(EVP_blake2s256(), key, strlen(key), input, strlen(input), NULL, NULL);
	printf("HMAC_BLAKE2s256: \n");
	print_output(digest, HASH_SIZE);
}
*/
void print_output(unsigned char* output, int len) {
	for(int i = 0; i < len; i++) {
		printf("%02x", output[i]);
	}
	printf("\n\n");
}

int main() {
	//unsigned char mssg[] = "The quick brown fox jumps over the lazy dog\0";
	//unsigned char mssg[] = "At vero eos et accusamus et iusto odio dignissimos ducimus qui blanditiis praesentium voluptatum deleniti atque corrupti quos dolores et quas molestias excepturi sint occaecati cupiditate non provident, similique sunt in culpa qui officia deserunt mollitia animi, id est laborum et dolorum fuga. Et harum quidem rerum facilis est et expedita distinctio. Nam libero tempore, cum soluta nobis est eligendi optio cumque nihil impedit quo minus id quod maxime placeat facere possimus, omnis voluptas assumenda est, omnis dolor repellendus. Temporibus autem quibusdam et aut officiis debitis aut rerum necessitatibus saepe eveniet ut et voluptates repudiandae sint et molestiae non recusandae. Itaque earum rerum hic tenetur a sapiente delectus, ut aut reiciendis voluptatibus maiores alias consequatur aut perferendis doloribus asperiores repellat.\0";
	unsigned char mssg[] = "hi world\0";
	/*strlen() allowed here since we know mssg is c string, but shouldn't be used with 
	bytes of other formats (it might save temperature data msg length to allow an extra 1 of 256 options in each message byte)*/
	int mssg_len = strlen(mssg); 
//	unsigned char mssg[] = ['0x01', '\0'];
//	unsigned char key[] = "1\0";
	unsigned char key256[] = "01234567890123456789012345678901\0";
	unsigned char key128[] = "0123456789012345\0";
//	unsigned char key256[] = ['0x01', '\0'];
	unsigned char iv128[] = "0123456789012345\0";
	unsigned char iv64[] = "01234567\0";
//	unsigned char iv128[] = ['0x02', '\0'];

	sha_1(mssg);
	sha_2(mssg);
	//hmac_sha2(mssg, key256);
	//hmac_blake2s(mssg, key256);

	aes256_ctr(mssg, key256, iv128, mssg_len);
	aes128_ctr(mssg, key128, iv128, mssg_len);
	cast5(mssg, key256, iv128, mssg_len);
	triple_des(mssg, key256, iv128, mssg_len);

	return 0;
}
