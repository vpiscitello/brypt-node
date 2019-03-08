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

#define BUFF_SIZE 512
#define HASH_SIZE 32
#define OUR_KEY_SIZE 32
class crypto{
	
	private:
		unsigned char plaintext[BUFF_SIZE];
		unsigned char key[OUR_KEY_SIZE];
		unsigned char* iv;
		unsigned char* digest;
		unsigned char ciphertext[BUFF_SIZE];
		unsigned char decryptedtext[BUFF_SIZE];
		unsigned char hash[HASH_SIZE];
		int ctxt_len;
		int ptxt_len;

	public:			
		crypto();
		~crypto();

		void clear_hash();
		void clear_ciphertext();
		void clear_decryptedtext();
		void clear_plaintext();
		void set_plaintext(unsigned char *);
		void set_our_key(unsigned char*, int);

		void triple_des_encrypt();
		void triple_des_decrypt();
		void cast5_encrypt();
		void cast5_decrypt();
		void aes_ctr_256_encrypt();
		void aes_ctr_256_decrypt();
		void aes_ctr_128_encrypt();
		void aes_ctr_128_decrypt();
		void sha_1(unsigned char*);
		void sha_2(unsigned char*);
		void hmac_sha2(unsigned char*);
		void hmac_blake2s(unsigned char*);
		void print_output(unsigned char*, int);

		unsigned char* get_plaintext();
		unsigned char* get_ciphertext();
		unsigned char* get_decryptedtext();
		unsigned char* get_hash();
		unsigned char* get_digest();

};

