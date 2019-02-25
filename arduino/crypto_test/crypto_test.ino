#include <Crypto.h>
#include <Hash.h>

#include <SHA256.h>
#include <BLAKE2s.h>
#include <AES.h>
#include <CTR.h>

#define HASH_SIZE 32
#define CRYPTO_AES_DEFAULT "AES-128-CTR"
#define CRYPTO_AES_DEFAULT "AES-256-CTR"

byte ctxt[128]; // Ciphertext
byte dtxt[128]; // Decrypted text

void hmac(Hash *h, byte *key, byte *result, byte *mssg){
  h->resetHMAC(key, strlen(key));
  h->update(mssg, strlen(mssg));
  h->finalizeHMAC(key, strlen(key), result, HASH_SIZE);
}

void hash(Hash *h, uint8_t *value, byte *mssg){
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

void sha2_test(byte *key, byte *mssg){
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

void blake2s_test(byte *key, byte *mssg){
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

void aes128_test(byte *key, byte *mssg, byte *iv){
  CTR<AES128> aes_ctr_128;
//  byte buffer[128];
  memset(ctxt, 0x00, sizeof(ctxt));

//  byte mssg[] = "The quick brown fox jumps over the lazy dog";
//  byte key[] = "01234567890123456789012345678901";
//  byte iv[] = "0123456789012345";

  crypto_feed_watchdog();
  aes_ctr_128.setKey(key, 16);
 // aes_ctr_128.setKey(key, (&aes_ctr_128)->keySize()); // This works??!
  aes_ctr_128.setIV(iv, 16);
 // aes_ctr_128.setIV(iv, (&aes_ctr_128)->ivSize());
  aes_ctr_128.setCounterSize(4);
  aes_ctr_128.encrypt(ctxt, mssg, strlen(mssg)+ 1);
  Serial.println("");
  Serial.println("AES-CTR-128 Ciphertext:");
 // Serial.print((char *)buffer);
  for (int x = 0; x < sizeof(ctxt); x++) {
    Serial.print(ctxt[x], HEX);
    Serial.print(" ");
  }
  Serial.println("");
}

void aes128_decrypt(byte *key, byte *iv, byte *buffer){
  CTR<AES128> aes_ctr_128;
  memset(buffer, 0x00, 128);
//
//  byte mssg[] = "The quick brown fox jumps over the lazy dog";
//  byte key[] = "01234567890123456789012345678901";
//  byte iv[] = "0123456789012345";

  crypto_feed_watchdog();
  aes_ctr_128.setKey(key, 16);
 // aes_ctr_128.setKey(key, (&aes_ctr_128)->keySize()); // This works??!
  aes_ctr_128.setIV(iv, 16);
 // aes_ctr_128.setIV(iv, (&aes_ctr_128)->ivSize());
  aes_ctr_128.setCounterSize(4);
  aes_ctr_128.decrypt(buffer, ctxt, 44);
  Serial.println("");
  Serial.println("AES-CTR-128 Decrypted text:");
 // Serial.print((char *)buffer);
  for (int x = 0; x < 128; x++) {
    Serial.print((char)buffer[x]);
 //   Serial.print(" ");
  }
  Serial.println("");
}

void aes256_test(byte *key, byte *mssg, byte *iv){
  CTR<AES256> aes_ctr_256;
  memset(ctxt, 0x00, sizeof(ctxt));

//  byte mssg[] = "The quick brown fox jumps over the lazy dog";
//  byte key[] = "01234567890123456789012345678901";
//  byte iv[] = "0123456789012345";

  crypto_feed_watchdog();
  aes_ctr_256.setKey(key, 32);
  aes_ctr_256.setIV(iv, 16);
  aes_ctr_256.setCounterSize(4);
  aes_ctr_256.encrypt(ctxt, mssg, strlen(mssg) + 1);
  Serial.println("");
  Serial.println("AES-CTR-256 Ciphertext:");
 // Serial.print((char *)buffer);
  for (int x = 0; x < sizeof(ctxt); x++) {
    Serial.print(ctxt[x], HEX);
    Serial.print(" ");
  }
  Serial.println("");
}

void aes256_decrypt(byte *key, byte *iv, byte *buffer){
  CTR<AES256> aes_ctr_256;
  memset(buffer, 0x00, 128);

//  byte mssg[] = "The quick brown fox jumps over the lazy dog";
//  byte key[] = "01234567890123456789012345678901";
//  byte iv[] = "0123456789012345";

  crypto_feed_watchdog();
  aes_ctr_256.setKey(key, 32);
  aes_ctr_256.setIV(iv, 16);
  aes_ctr_256.setCounterSize(4);
  aes_ctr_256.decrypt(buffer, ctxt, 44);
  Serial.println("");
  Serial.println("AES-CTR-256 Decrypted text:");
 // Serial.print((char *)buffer);
  for (int x = 0; x < 128; x++) {
    Serial.print((char)buffer[x]);
 //   Serial.print(" ");
  }
  Serial.println("");
}

void setup() {
  // put your main code here, to run repeatedly:

}

void loop() {

 /* unsigned char mssg[] = "The quick brown fox jumps over the lazy dog";
  unsigned char key[] = "01234567890123456789012345678901";
  unsigned char key128[] = "0123456789012345";
  unsigned char iv[] = "0123456789012345";
*/
  byte mssg[] = "The quick brown fox jumps over the lazy dog";
  byte key256[] = "01234567890123456789012345678901";
  byte key128[] = "0123456789012345";
  byte iv[] = "0123456789012345";
  byte buffer[128];

  sha2_test(key256, mssg);
  
  blake2s_test(key256, mssg);
  aes128_test(key128, mssg, iv);
  aes128_decrypt(key128, iv, buffer);
  
  aes256_test(key256, mssg, iv);
  aes256_decrypt(key256, iv, buffer);

  delay(10000);
  /*(&sha256)->reset();
  (&sha256)->update(mssg, sizeof(mssg));
  (&sha256)->finalize(buffer, (&sha256)->hashSize());*/
  
}

