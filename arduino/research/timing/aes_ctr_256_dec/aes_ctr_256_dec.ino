#include <SoftwareSerial.h>
#include <Crypto.h>
#include <AES.h>
#include <CTR.h>

#define HASH_SIZE 32

byte ctxt[44] = {0x09, 0xf1, 0xe4, 0x64, 0x98, 0x3a, 0x7d, 0x25, 0x30, 0x5d, 0x5b,
                 0x86, 0x53, 0x86, 0xe4, 0x77, 0xae, 0xc3, 0x59, 0x54, 0xfc, 0x08,
                 0x79, 0xce, 0xbf, 0xe9, 0xbd, 0x04, 0x13, 0x27, 0x07, 0x7c, 0x21,
                 0xfb, 0xc9, 0x60, 0x80, 0x7c, 0x86, 0x6c, 0xad, 0x02, 0x85, 0xb5};

void aes256_dec(byte *key, byte *iv, byte *buffer){
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
 /* Serial.println("");
  Serial.println("AES-CTR-256 Decrypted text:");
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
  byte key256[] = "01234567890123456789012345678901";
  byte iv[] = "0123456789012345";
  byte buffer[128];

  t_start = millis();
  for(unsigned long i = 0; i < 5000/*30000000*/; i++){
    aes256_dec(key256, iv, buffer);
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
