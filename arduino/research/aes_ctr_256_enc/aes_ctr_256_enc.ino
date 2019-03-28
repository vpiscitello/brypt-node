#include <Crypto.h>
#include <AES.h>
#include <CTR.h>

#define HASH_SIZE 32

byte ctxt[128]; // Ciphertext

void aes256_enc(byte *key, byte *mssg, byte *iv){
  CTR<AES256> aes_ctr_256;
  memset(ctxt, 0x00, sizeof(ctxt));

  crypto_feed_watchdog();
  aes_ctr_256.setKey(key, 32);
  aes_ctr_256.setIV(iv, 16);
  aes_ctr_256.setCounterSize(4);
  aes_ctr_256.encrypt(ctxt, mssg, strlen((const char*)mssg) + 1);
}

void setup() {}

void loop() {
  byte mssg[] = "The quick brown fox jumps over the lazy dog";
  byte key256[] = "01234567890123456789012345678901";
  byte iv[] = "0123456789012345";

	while(1){
    aes256_enc(key256, mssg, iv);
	}
}
