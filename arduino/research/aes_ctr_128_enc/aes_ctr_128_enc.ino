#include <SoftwareSerial.h>
#include <Crypto.h>
#include <AES.h>
#include <CTR.h>

#define HASH_SIZE 32

byte ctxt[128]; // Ciphertext

void aes128_enc(byte *key, byte *mssg, byte *iv){
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
 // Serial.println("");
 // Serial.println("AES-CTR-128 Ciphertext:");
 // Serial.print((char *)buffer);
 /* for (int x = 0; x < sizeof(ctxt); x++) {
    Serial.print(ctxt[x], HEX);
    Serial.print(" ");
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
  byte mssg[] = "The quick brown fox jumps over the lazy dog";
  byte key128[] = "0123456789012345";
  byte iv[] = "0123456789012345";

  t_start = millis();
  for(unsigned long i = 0; i < 5000/*30000000*/; i++){
    aes128_enc(key128, mssg, iv);
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
