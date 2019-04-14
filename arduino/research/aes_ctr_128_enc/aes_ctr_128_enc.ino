#include <Crypto.h>
#include <AES.h>
#include <CTR.h>

#define HASH_SIZE 32

byte ctxt[128]; // Ciphertext

void aes128_enc(byte *key, byte *mssg, byte *iv){
  CTR<AES128> aes_ctr_128;
  memset(ctxt, 0x00, sizeof(ctxt));

  crypto_feed_watchdog();
  aes_ctr_128.setKey(key, 16);
  aes_ctr_128.setIV(iv, 16);
  aes_ctr_128.setCounterSize(4);
  aes_ctr_128.encrypt(ctxt, mssg, strlen((const char*)mssg)+ 1);
}

void setup() {}

void loop() {
  byte mssg[] = "The quick brown fox jumps over the lazy dog";
  byte key128[] = "0123456789012345";
  byte iv[] = "0123456789012345";

	while(1){
    aes128_enc(key128, mssg, iv);
	}
}
