#include <stdio.h>	// Only needed for printf
#include <string.h>
#include <openssl/sha.h> // For SHA1 and SHA2
#include <openssl/evp.h>	// For HMAC
#include <openssl/hmac.h>	

void sha_1(unsigned char*);
void sha_2(unsigned char*);
void hmac_sha2(unsigned char*, unsigned char*);
void hmac_blake2s(unsigned char*, unsigned char*);
void print_output(unsigned char*);

void sha_1(unsigned char* input){
	unsigned char output[30];
	SHA1(input, strlen(input), output);
	printf("SHA1: \n");
	print_output(output);
}

void sha_2(unsigned char* input){
	unsigned char output[30];
	SHA256(input, strlen(input), output);
	printf("SHA2: \n");
	print_output(output);
}

void hmac_sha2(unsigned char* input, unsigned char* key){
	unsigned char* digest;
	digest = HMAC(EVP_sha256(), key, strlen(key), input, strlen(input), NULL, NULL);
	printf("HMAC_SHA2: \n");
	print_output(digest);
}

void hmac_blake2s(unsigned char* input, unsigned char* key){
	unsigned char* digest;
	digest = HMAC(EVP_blake2s256(), key, strlen(key), input, strlen(input), NULL, NULL);
	printf("HMAC_BLAKE2s256: \n");
	print_output(digest);
}

void print_output(unsigned char* output) {
	for(int i = 0; i < strlen(output); i++) {
		printf("%02x", output[i]);
	}
	printf("\n\n");
}

int main() {
	unsigned char mssg[] = "This is the message.";
	unsigned char key[] = "93GWBUGO8T";
	sha_1(mssg);
	sha_2(mssg);
	hmac_sha2(mssg, key);
	hmac_blake2s(mssg, key);

	return 0;
}

