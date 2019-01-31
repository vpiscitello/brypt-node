#include "./crypto.h"

void spacer(void);

int main() {

	crypto c;

	c.sha_1(c.get_plaintext());	
	spacer();
	
	c.sha_2(c.get_plaintext());	
	spacer();
	
	c.hmac_sha2(c.get_plaintext());	
	spacer();

	c.hmac_blake2s(c.get_plaintext());	
	spacer();
	
	c.aes_ctr_256_encrypt();
	c.aes_ctr_256_decrypt();
	spacer();

	c.set_our_key((unsigned char *)"0123456789012345",16);
	c.aes_ctr_128_encrypt();
	c.aes_ctr_128_decrypt();
	spacer();
	
	c.set_our_key((unsigned char *)"01234567890123456789012345678901",32);
	c.cast5_encrypt();
	c.cast5_decrypt();
	spacer();

	c.set_our_key((unsigned char *)"012345678901234567890123",24);
	c.triple_des_encrypt();
	c.triple_des_decrypt();

	/*	aes_ctr_decrypt(ciphertext, decryptedtext, key256, iv128);
	spacer();
	cast5_encrypt(plaintext, ciphertext, key256, iv128);
	cast5_decrypt(ciphertext, decryptedtext, key256, iv128);
	spacer();
	triple_des_encrypt(plaintext, ciphertext, key256, iv128);
	triple_des_decrypt(ciphertext, decryptedtext, key256, iv128);
*/
	return 0;
}

void spacer(void){
 printf("\n-----------------------------------------------------\n");
}
