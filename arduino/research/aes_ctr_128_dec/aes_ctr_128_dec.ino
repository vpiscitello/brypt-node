#include <SoftwareSerial.h>
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
 // aes_ctr_128.setKey(key, (&aes_ctr_128)->keySize()); // This works??!
  aes_ctr_128.setIV(iv, 16);
 // aes_ctr_128.setIV(iv, (&aes_ctr_128)->ivSize());
  aes_ctr_128.setCounterSize(4);
  aes_ctr_128.decrypt(buffer, ctxt, 44);

/*Serial.println("");
  Serial.println("AES-CTR-128 Ciphertext:");
 // Serial.print((char *)buffer);
  for (int x = 0; x < sizeof(ctxt); x++) {
    Serial.print(ctxt[x], HEX);
    Serial.print(" ");
  }
  Serial.println("");
  
  Serial.println("");
  Serial.println("AES-CTR-128 Decrypted text:");
 // Serial.print((char *)buffer);
  for (int x = 0; x < 128; x++) {
    Serial.print((char)buffer[x]);
 //   Serial.print(" ");
  }
  Serial.println("");
  */
}

SoftwareSerial mySerial(10, 11); //rx, tx

void setup() {
  // put your setup code here, to run once:

 /* Serial.begin(57600);
  while(!Serial){
    ;
  }*/
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long long t_start = 0;
  unsigned long long t_end = 0;
  unsigned long duration;
  byte key128[] = "0123456789012345";
  byte iv[] = "0123456789012345";
  byte buffer[128];

  t_start = millis();
  for(unsigned long i = 0; i < 5000/*30000000*/; i++){
    aes128_dec(key128, iv, buffer);
  }
  
  t_end = millis();

  duration = (unsigned long)(t_end - t_start);
  Serial.println("\nDone");
  Serial.print((unsigned long)t_end, DEC);
  Serial.println("\nStart");
  Serial.print((unsigned long)t_start, DEC);
  Serial.println("\nDuration:");
  Serial.print((unsigned long)duration, DEC);
  Serial.println("\n");
  
  /*while(1){
    ;
  }*/
}
