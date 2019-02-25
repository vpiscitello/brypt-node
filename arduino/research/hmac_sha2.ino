#include <Crypto.h>
#include <Hash.h>
#include <SHA256.h>

#define HASH_SIZE 32

void hmac(Hash *h, byte *key, byte *result, byte *mssg){
  h->resetHMAC(key, strlen(key));
  h->update(mssg, strlen(mssg));
  h->finalizeHMAC(key, strlen(key), result, HASH_SIZE);
}

void hash(Hash *h, uint8_t *value, byte *mssg){
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

/*  Serial.println("");
  Serial.println("SHA2 HMAC:");

  for (int x = 0; x < sizeof(result); x++) {
    Serial.print(result[x], HEX);
  }
*/
 /* for (int x = 0; x < sizeof(result); x++) {
    Serial.print(result[x], BIN);
  }*/
}

void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned int num_hashes = 0;
  byte mssg[] = "The quick brown fox jumps over the lazy dog";
  byte key256[] = "01234567890123456789012345678901";
  unsigned long end_time, start_time = -1;
  
  start_time = millis();
//  Serial.println("\nStart");
//  Serial.println("\n");
//  Serial.print(start_time);
  while( num_hashes < 100){
    sha2_test(key256, mssg);
    num_hashes = num_hashes + 1;
  }
  end_time = millis();
  Serial.println("\nDone");
  Serial.print(end_time);
  Serial.println("\nStart");
  Serial.print(start_time);
  while(1){
    ;
  }
}
