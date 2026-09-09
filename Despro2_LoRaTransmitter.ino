#include <SPI.h>
#include <LoRa.h>

void setup() {
  Serial.begin(115200);
  LoRa.begin(433E6);
  // put your setup code here, to run once:

}

void loop() {
  LoRa.beginPacket();
  LoRa.print("HELLO");
  LoRa.endPacket();
  delay(2000);
  // put your main code here, to run repeatedly:

}
