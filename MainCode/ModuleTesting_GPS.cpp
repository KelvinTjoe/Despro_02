#include <TinyGPSPlus.h>

#define GPS_RX 16  // ESP32 pin ini menerima data dari TX modul GPS
#define GPS_TX 17  // ESP32 pin ini mengirim ke RX modul GPS

TinyGPSPlus gps;

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
}

void loop() {
  while (Serial2.available() > 0) {
    gps.encode(Serial2.read());
  }

  if (gps.location.isUpdated()) {
    Serial.print("Latitude: ");
    Serial.println(gps.location.lat(), 6);
    Serial.print("Longitude: ");
    Serial.println(gps.location.lng(), 6);
  }
}

