#include <Crypto.h>
#include <Hash.h>

#include <SHA256.h>
#include <BLAKE2s.h>
#include <AES.h>
#include <CTR.h>

#define HASH_SIZE 32
#define CRYPTO_AES_DEFAULT "AES-256-CTR"

void hmac(Hash *h, unsigned char *key, byte *result, unsigned char *mssg){
  h->resetHMAC(key, strlen(key));
  h->update(mssg, strlen(mssg));
  h->finalizeHMAC(key, strlen(key), result, HASH_SIZE);
}

void hash(Hash *h, uint8_t *value, unsigned char *mssg){
 /* h->reset();
  h->update(mssg, sizeof(mssg));
  h->finalize(buffer, h->hashSize()); 
*/
    size_t inc = 1;
    size_t size = strlen(mssg);
    size_t posn, len;
  //  uint8_t value[HASH_SIZE];

    h->reset();
    for (posn = 0; posn < size; posn += inc) {
        len = size - posn;
        if (len > inc)
            len = inc;
        h->update(mssg + posn, len);
    }
    h->finalize(value, HASH_SIZE);
}

void sha2_test(unsigned char *key, unsigned char *mssg){
  SHA256 sha256; 
  uint8_t buffer[HASH_SIZE];
  byte result[HASH_SIZE];

  memset(buffer, 0x00, sizeof(buffer));
  memset(result, 0x00, sizeof(result));
   
  hash(&sha256, buffer, mssg);
  hmac(&sha256, key, result, mssg); 

  Serial.println("");
  Serial.println("SHA2 Hash:");
 // Serial.print((char *)buffer);
  
  for (int x = 0; x < sizeof(buffer); x++) {
    Serial.print(buffer[x], HEX);
  }

  Serial.println("");
  Serial.println("SHA2 HMAC:");

  for (int x = 0; x < sizeof(result); x++) {
    Serial.print(result[x], HEX);
  }

 /* for (int x = 0; x < sizeof(result); x++) {
    Serial.print(result[x], BIN);
  }*/
}

void blake2s_test(unsigned char *key, unsigned char *mssg){
  BLAKE2s blake2s; 
  byte buffer[128];
  byte result[HASH_SIZE];

  memset(buffer, 0x00, sizeof(buffer));
  memset(result, 0x00, sizeof(result));
   
  hash(&blake2s, buffer, mssg);
  hmac(&blake2s, key, result, mssg); 
    
  Serial.println("");
  Serial.println("BLAKE2s Hash:");
 // Serial.print((char *)buffer);
  
  for (int x = 0; x < sizeof(buffer); x++) {
    Serial.print(buffer[x], HEX);
  }

  Serial.println("");
  Serial.println("BLAKE2s HMAC:");

  for (int x = 0; x < sizeof(result); x++) {
    Serial.print(result[x], HEX);
  }
}

/*void enc(BlockCipher *c, char *key, char *mssg, byte *buffer){
  crypto_feed_watchdog();
  c->setKey(key, c->keySize());
  c->encryptBlock(buffer, mssg);
}*/

void aes256_test(unsigned char *k, unsigned char *m, unsigned char *i){
  CTR<AES256> aes_ctr_256;
  byte buffer[128];
  memset(buffer, 0x00, sizeof(buffer));

  byte mssg[] = "The quick brown fox jumps over the lazy dog";
  byte key[] = "01234567890123456789012345678901";
  byte iv[] = "0123456789012345";

  crypto_feed_watchdog();
  aes_ctr_256.setKey(key, 32);
  aes_ctr_256.setIV(iv, 16);
  aes_ctr_256.setCounterSize(4);
  aes_ctr_256.encrypt(buffer, mssg, sizeof(mssg));
  Serial.println("AES-CTR Ciphertext:");
 // Serial.print((char *)buffer);
  for (int x = 0; x < sizeof(buffer); x++) {
    Serial.print(buffer[x], HEX);
    Serial.print(" ");
  }
  Serial.println("");
}

void setup() {
  // put your main code here, to run repeatedly:

}

void loop() {

  unsigned char mssg[] = "The quick brown fox jumps over the lazy dog";
  unsigned char key[] = "01234567890123456789012345678901";
  unsigned char iv[] = "0123456789012345";

  sha2_test(key, mssg);
  
  blake2s_test(key, mssg);
  aes256_test(key, mssg, iv);

  delay(10000);
  /*(&sha256)->reset();
  (&sha256)->update(mssg, sizeof(mssg));
  (&sha256)->finalize(buffer, (&sha256)->hashSize());*/
  
}

