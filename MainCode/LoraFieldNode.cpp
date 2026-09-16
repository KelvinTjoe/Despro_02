#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>

#include "config.h"
#include "packet.h"

// File .cpp tidak dapat prototipe otomatis seperti .ino,
// jadi setiap fungsi harus dideklarasikan sebelum dipakai.
void bacaGPS();
void periksaTombol();
void terimaPaket();
void laporGPS();
void kirimStatus(StatusPersonel status);

TinyGPSPlus gps;

// Posisi valid terakhir. Hanya diperbarui saat GPS benar-benar fix,
// sehingga nilainya tetap terpakai ketika sinyal satelit hilang.
float lastLat = 0.0f;
float lastLon = 0.0f;
unsigned long lastFixTime = 0;
bool pernahFix = false;

struct Tombol {
  uint8_t pin;
  StatusPersonel status;
  bool bacaanMentah;
  bool bacaanStabil;
  unsigned long waktuBerubah;
  unsigned long mulaiTahan;
  bool sudahKirim;
};

Tombol tombol[] = {
  { BUTTON_AMAN,    STATUS_AMAN,    false, false, 0, 0, false },
  { BUTTON_SIAGA,   STATUS_SIAGA,   false, false, 0, 0, false },
  { BUTTON_BANTUAN, STATUS_BANTUAN, false, false, 0, 0, false },
};

const int JUMLAH_TOMBOL = sizeof(tombol) / sizeof(tombol[0]);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("=== Field Node %d ===\n", NODE_ID);

  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    pinMode(tombol[i].pin, INPUT_PULLUP);
  }

  //inisialisasi GPS NEO-6M di UART2, pin RX/TX sesuai config.h
  //cold start minimum 30 detik
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS: UART2 siap, menunggu koordinat fix dari satelit");

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  //jika lora gagal init, loop di sini selamanya 
  //daripada menampilakn pesan error terus menerus
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa: init GAGAL, cek wiring SPI dan catu daya modul");
    while (1) {
      delay(1000);
    }
  } 

  //inisialisasi parameter radio LoRa, set parameter sesuai config.h
  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setTxPower(LORA_TX_POWER);

  Serial.printf("LoRa: OK pada %d MHz, SF%d\n", (int)(LORA_FREQ / 1E6), LORA_SF);
  Serial.printf("Tahan salah satu tombol selama %lu detik untuk mengirim status\n", HOLD_DURATION/1000);
}

void loop() {
  bacaGPS();
  periksaTombol();
  terimaPaket();
  laporGPS();
}

void bacaGPS() {
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      if (gps.location.isValid() && gps.location.isUpdated()) {
        lastLat = gps.location.lat();
        lastLon = gps.location.lng();
        lastFixTime = millis();
        pernahFix = true;
      }
    }
  }
}

void periksaTombol() {
  unsigned long now = millis();

  for (int i = 0; i < JUMLAH_TOMBOL; i++) {
    // Referensi, bukan salinan, supaya perubahan di bawah benar-benar
    // tersimpan ke array dan tidak hilang di akhir iterasi.
    Tombol &t = tombol[i];

    bool mentah = (digitalRead(t.pin) == LOW);

    if (mentah != t.bacaanMentah) {
      t.bacaanMentah = mentah;
      t.waktuBerubah = now;
    }

    if (now - t.waktuBerubah >= DEBOUNCE_MS && mentah != t.bacaanStabil) {
      t.bacaanStabil = mentah;

      if (mentah) {
        t.mulaiTahan = now;
        t.sudahKirim = false;
        Serial.printf("[%s] mulai ditahan...\n", namaStatus(t.status));
      } else if (!t.sudahKirim) {
        Serial.printf("[%s] dilepas terlalu cepat, dibatalkan\n", namaStatus(t.status));
      }
    }

    if (t.bacaanStabil && !t.sudahKirim && now - t.mulaiTahan >= HOLD_DURATION) {
      kirimStatus(t.status);
      t.sudahKirim = true;
    }
  }
}

void kirimStatus(StatusPersonel status) {
  float lat = 0.0f;
  float lon = 0.0f;
  KualitasPosisi kualitas = POS_NOFIX;

  if (pernahFix) {
    lat = lastLat;
    lon = lastLon;
    kualitas = (millis() - lastFixTime <= GPS_STALE_AGE) ? POS_FIX : POS_STALE;
  }

  String paket = buatPaket(NODE_ID, status, lat, lon, kualitas);

  LoRa.beginPacket();
  LoRa.print(paket);
  LoRa.endPacket();

  Serial.printf("TERKIRIM: %s\n", paket.c_str());

  if (kualitas == POS_NOFIX) {
    Serial.println("  peringatan: GPS belum pernah fix, koordinat dikirim 0,0");
  } else if (kualitas == POS_STALE) {
    Serial.printf("  peringatan: posisi berumur %lu detik\n",
                  (millis() - lastFixTime) / 1000);
  }
}

void terimaPaket() {
  int panjang = LoRa.parsePacket();
  if (panjang == 0) {
    return;
  }

  String pesan;
  while (LoRa.available()) {
    pesan += (char)LoRa.read();
  }

  Serial.printf("DITERIMA: %s (RSSI %d dBm)\n", pesan.c_str(), LoRa.packetRssi());
}

// Hanya cetak ke Serial untuk debugging, tidak mengirim paket LoRa.
void laporGPS() {
  static unsigned long terakhirLapor = 0;
  unsigned long now = millis();

  if (now - terakhirLapor < GPS_REPORT_MS) {
    return;
  }
  terakhirLapor = now;

  if (!pernahFix) {
    Serial.printf("GPS: belum fix (%d satelit terlihat)\n", gps.satellites.value());
    return;
  }

  Serial.printf("GPS: %.6f, %.6f | %d satelit | umur %lu detik\n",
                lastLat, lastLon, gps.satellites.value(),
                (now - lastFixTime) / 1000);
}
