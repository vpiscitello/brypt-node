#include <Crypto.h>
#include <Hash.h>
#include <BLAKE2s.h>

#define HASH_SIZE 32

void hmac(Hash *h, byte *key, byte *result, byte *mssg){
  h->resetHMAC(key, strlen((char*)key));
  h->update(mssg, strlen((char*)mssg));
  h->finalizeHMAC(key, strlen((char*)key), result, HASH_SIZE);
}

void hash(Hash *h, uint8_t *value, byte *mssg){
    size_t inc = 1;
    size_t size = strlen((char*)mssg);
    size_t posn, len;

    h->reset();
    for (posn = 0; posn < size; posn += inc) {
        len = size - posn;
        if (len > inc)
            len = inc;
        h->update(mssg + posn, len);
    }
    h->finalize(value, HASH_SIZE);
}

void blake2s_test(byte *key, byte *mssg){
  BLAKE2s blake2s; 
  byte buffer[128];
  byte result[HASH_SIZE];

  memset(buffer, 0x00, sizeof(buffer));
  memset(result, 0x00, sizeof(result));
   
  hash(&blake2s, buffer, mssg);
  hmac(&blake2s, key, result, mssg); 
}

void setup() {}

void loop() {
  byte mssg[] = "The quick brown fox jumps over the lazy dog";
  byte key256[] = "01234567890123456789012345678901";

	while(1){
    blake2s_test(key256, mssg);
	}
}
