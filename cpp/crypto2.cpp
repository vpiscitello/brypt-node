#include <iostream>	// Only needed for printf
#include <cstdlib>	// For rand()
#include <cstring>
#include <openssl/sha.h> // For SHA1 and SHA2
#include <openssl/evp.h>	// For HMAC
#include <openssl/hmac.h>	

void cast5(const unsigned char* input, const unsigned char* k, const unsigned char* iv); 
void hmac_sha2(const unsigned char*, const unsigned char*);
void print_output(unsigned char*);

/***************CIPHERS*****************/
void cast5(const unsigned char* input, const unsigned char* k, const unsigned char* iv) {
  int length;
  unsigned char* ciphertext;
  EVP_CIPHER_CTX *ctx;
						
  ctx = EVP_CIPHER_CTX_new();
								
  EVP_EncryptInit_ex(ctx, EVP_cast5_cbc(), NULL, k, iv);
  EVP_EncryptUpdate(ctx, ciphertext, &length, input, strlen((const char*)input));
 print_output(ciphertext);
 	print_output(ciphertext+1);
	 
	 EVP_EncryptFinal_ex(ctx, ciphertext+length, &length);
 // EVP_CIPHER_CTX_free(ctx);
												
  printf("CAST5 Plaintext:\n");
  printf("%s\n\n", input);
  printf("CAST5 Ciphertext (hex representation):\n");
//  print_output(ciphertext);
}

/***************HASHES*****************/
void hmac_sha2(const unsigned char* input, const unsigned char* key){
	unsigned char* digest;
	digest = HMAC(EVP_sha256(), key, strlen((const char*)key), input, strlen((const char*)input), NULL, NULL);
	printf("HMAC_SHA2: \n");
	print_output(digest);
}

void print_output(unsigned char* output) {
	for(int i = 0; i < strlen((const char*)output); i++) {
		printf("%02x", output[i]);
	}
	printf("\n\n");
	printf("Length: %d\n", (int)strlen((const char*)output));
}

int main() {
//	const char mssg[] = "This is the message.\0";
	static unsigned char mssg[] = "message";
  static unsigned char key[] = "0";
	static unsigned char key256[] = "01234567890123456789012345678901";
	static unsigned char iv128[] = "0123456789012345";

	cast5(mssg, key256, iv128);
	hmac_sha2(mssg, key);

	return 0;
}

