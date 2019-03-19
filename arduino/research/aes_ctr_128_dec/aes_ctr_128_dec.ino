#include <Crypto.h>
#include <AES.h>
#include <CTR.h>

#define HASH_SIZE 32
#define CRYPTO_AES_DEFAULT "AES-128-CTR"

byte ctxt[44] = {0x0b, 0x15, 0x95, 0x9f, 0x61, 0x4f, 0xe5, 0x29, 0x8d, 0xda, 0xcf,
                  0xeb, 0x69, 0xdb, 0x55, 0x0a, 0x00, 0x58, 0x58, 0xfa, 0x7c, 0x6f,
                  0xd0, 0x52, 0x52, 0x53, 0x73, 0x61, 0x1b, 0x08, 0x2e, 0x03, 0xa2,
                  0xd1, 0x16, 0x0c, 0x98, 0x4b, 0xf5, 0xc7, 0x43, 0x6c, 0xca, 0xd5}; // Ciphertext

void aes128_dec(byte *key, byte *iv, byte *buffer){
  CTR<AES128> aes_ctr_128;
  memset(buffer, 0x00, 128);

  crypto_feed_watchdog();
  aes_ctr_128.setKey(key, 16);
  aes_ctr_128.setIV(iv, 16);
  aes_ctr_128.setCounterSize(4);
  aes_ctr_128.decrypt(buffer, ctxt, 44);

}


void setup() {}

void loop() {
  byte key128[] = "0123456789012345";
  byte iv[] = "0123456789012345";
  byte buffer[128];

	while(1){
    aes128_dec(key128, iv, buffer);
	}
}
