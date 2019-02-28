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
	
	return 0;
}

void spacer(void){
 printf("\n-----------------------------------------------------\n");
}
