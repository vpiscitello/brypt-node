#include "./crypto.h"
#include "../misc/DHKAgeneration.cpp"
#include "../misc/ECDHgeneration.cpp"
void spacer(void);
EVP_PKEY* gen();
int main() {

	crypto c;

	c.sha_1(c.get_plaintext());	
	spacer();
	
	c.sha_2(c.get_plaintext());	
	spacer();
	
	c.hmac_sha2(c.get_plaintext());	
	spacer();

	//c.hmac_blake2s(c.get_plaintext());	
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
	//c.set_local_ka(local_comp);
	//c.set_remote_ka(remote_comp);
	//c.modded_DHKA();
	
	EVP_PKEY *local_key;
	EVP_PKEY *remote_key;
	EVP_PKEY *params;
	params = genDHKAParams();
	local_key = genDHKA(params);
	remote_key = genDHKA(params);
	crypto wrapper;
	wrapper.modded_DHKA(local_key, remote_key);
	wrapper.modded_DHKA(local_key, remote_key);
	static BIO* out = BIO_new_fp(stdout, BIO_NOCLOSE);
	EVP_PKEY_print_private(out,local_key,3,NULL);

	EVP_PKEY *local_key2;
	EVP_PKEY *remote_key2;
	EVP_PKEY *params2;
	params2 = genECDHParams();
	printf("ec param'd\n");
	local_key2 = genECDH(params2);
	remote_key2 = genECDH(params2);
	printf("ran ec twice\n");
	crypto wrapper2;
	wrapper2.modded_ECDH(local_key2, remote_key2);
	wrapper2.modded_ECDH(local_key2, remote_key2);
	//static BIO* out2 = BIO_new_fp(stdout, BIO_NOCLOSE);
	EVP_PKEY_print_private(out,local_key2,3,NULL);
	return 0;
}

void spacer(void){
 printf("\n-----------------------------------------------------\n");
}
