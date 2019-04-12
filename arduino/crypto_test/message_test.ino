#include "message.hpp"
void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
  Message tmp("1", "2", 1, 0, "hiya!", 4);
  Serial.println(tmp.hmac("Hiya!"));
  break
}
