#include <SPI.h>
#include <LoRa.h>

String lastMessage = "";
int lastRssi = 0;

int to_read = 0;

void setup() {
  Serial.begin(115200);
  LoRa.begin(433E6);
}

void loop() {
  to_read = LoRa.parsePacket();
  if (to_read != 0) {
    lastRssi = LoRa.packetRssi();
    lastMessage = "";

    while (to_read != 0) {
      char byte_read = (char)LoRa.read();
      lastMessage += byte_read;
      to_read--;
    }

    Serial.println(lastMessage);
    Serial.println(lastRssi);
  }
}