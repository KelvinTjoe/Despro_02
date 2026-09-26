#include <Arduino.h>
#include <TinyGPSPlus.h>

#include "config.h"

// ============================================================
// UJI MODUL GPS NEO-6M, berdiri sendiri tanpa LoRa.
//
// Menjawab tiga pertanyaan, berurutan dari yang paling mendasar:
//
//   1. Apakah modulnya terpasang benar? Dijawab charsProcessed():
//      selama nilainya 0, tidak ada satu byte pun sampai ke ESP32,
//      jadi masalahnya kabel, baud, atau RX/TX tertukar. Jumlah
//      satelit tidak berguna untuk ini, karena 0 satelit bisa
//      berarti "sedang mencari" ATAU "modul tidak bicara".
//
//   2. Apakah datanya utuh? Dijawab perbandingan passedChecksum()
//      dan failedChecksum(). Checksum yang banyak gagal menandakan
//      baud salah atau sambungan longgar.
//
//   3. Apakah sudah fix? Dijawab location.isValid(). Sebelum fix
//      posisi, GPS biasanya sudah lebih dulu mengunci waktu dari
//      satelit, jadi time.isValid() dipakai sebagai tanda bahwa
//      modul benar-benar sedang bekerja dan tinggal ditunggu.
//
// Cold start di tempat terbuka butuh sekitar 30 detik sampai
// beberapa menit. Di dalam ruangan sering tidak pernah fix sama
// sekali, jadi ujilah dekat jendela atau di luar.
// ============================================================

// Setel 1 untuk menumpahkan kalimat NMEA mentah ke Serial. Berguna
// kalau charsProcessed() bergerak tapi tidak ada yang masuk akal.
#define TAMPILKAN_NMEA  0

void bacaGPS();
void periksaModul();
void laporBerkala();

TinyGPSPlus gps;

unsigned long waktuMulai = 0;
bool sudahLaporModul = false;
bool pernahFix = false;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== UJI MODUL GPS NEO-6M ===");

  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

  Serial.printf("UART2: RX pin %d, TX pin %d, %d baud\n", GPS_RX, GPS_TX, GPS_BAUD);
  Serial.println("Catatan wiring: TX modul GPS masuk ke pin RX ESP32.");
  Serial.println("Menunggu data dari modul...");
  Serial.println();

  waktuMulai = millis();
}

void loop() {
  bacaGPS();
  periksaModul();
  laporBerkala();
}

void bacaGPS() {
  while (Serial2.available() > 0) {
    char c = Serial2.read();

#if TAMPILKAN_NMEA
    Serial.write(c);
#endif

    if (gps.encode(c)) {
      if (gps.location.isValid()) {
        pernahFix = true;
      }
    }
  }
}

// Vonis sekali saja, 5 detik setelah boot. Pada 9600 baud, modul yang
// sehat sudah mengirim ratusan karakter dalam rentang itu.
void periksaModul() {
  if (sudahLaporModul || millis() - waktuMulai < 5000) {
    return;
  }
  sudahLaporModul = true;

  if (gps.charsProcessed() < 10) {
    Serial.println("MODUL   : TIDAK TERDETEKSI");
    Serial.println("  tidak ada data NMEA yang masuk sama sekali. Periksa:");
    Serial.println("  - TX modul GPS harus ke pin RX ESP32, bukan TX ke TX");
    Serial.printf("  - baud modul harus %d (bawaan NEO-6M)\n", GPS_BAUD);
    Serial.println("  - VCC dan GND modul benar-benar tersambung");
    Serial.println("  - LED pada modul menyala");
    Serial.println();
    return;
  }

  Serial.printf("MODUL   : TERDETEKSI, %lu karakter diterima dalam 5 detik\n",
                gps.charsProcessed());
  Serial.println("  modul bicara dengan benar, sekarang tinggal menunggu fix");
  Serial.println();
}

void laporBerkala() {
  static unsigned long terakhirLapor = 0;
  unsigned long now = millis();

  if (now - terakhirLapor < GPS_REPORT_MS) {
    return;
  }
  terakhirLapor = now;

  unsigned long detik = (now - waktuMulai) / 1000;

  if (gps.location.isValid()) {
    Serial.printf("FIX     : %.6f, %.6f\n",
                  gps.location.lat(), gps.location.lng());
    Serial.printf("  umur posisi  : %lu ms\n", gps.location.age());

    if (gps.satellites.isValid()) {
      Serial.printf("  satelit      : %lu\n", gps.satellites.value());
    }
    if (gps.hdop.isValid()) {
      // HDOP di bawah 2 bagus, di atas 5 posisinya kasar.
      Serial.printf("  hdop         : %.2f\n", gps.hdop.hdop());
    }
    if (gps.altitude.isValid()) {
      Serial.printf("  ketinggian   : %.1f m\n", gps.altitude.meters());
    }
    if (gps.time.isValid() && gps.date.isValid()) {
      Serial.printf("  waktu UTC    : %04d-%02d-%02d %02d:%02d:%02d\n",
                    gps.date.year(), gps.date.month(), gps.date.day(),
                    gps.time.hour(), gps.time.minute(), gps.time.second());
    }
    Serial.println();
    return;
  }

  // Belum fix. Yang dicetak di sini adalah bukti apakah modul sedang
  // bekerja atau diam saja, bukan sekadar "belum fix".
  Serial.printf("BELUM FIX (%lu detik sejak nyala)\n", detik);
  Serial.printf("  karakter NMEA : %lu\n", gps.charsProcessed());
  Serial.printf("  checksum      : %lu lolos, %lu gagal\n",
                gps.passedChecksum(), gps.failedChecksum());
  Serial.printf("  kalimat berfix: %lu\n", gps.sentencesWithFix());

  if (gps.satellites.isValid()) {
    Serial.printf("  satelit       : %lu terlihat\n", gps.satellites.value());
  } else {
    Serial.println("  satelit       : belum dilaporkan modul");
  }

  if (gps.time.isValid()) {
    Serial.printf("  waktu UTC     : %02d:%02d:%02d (waktu sudah terkunci, "
                  "posisi menyusul)\n",
                  gps.time.hour(), gps.time.minute(), gps.time.second());
  } else {
    Serial.println("  waktu         : belum terkunci");
  }

  if (gps.charsProcessed() == 0) {
    Serial.println("  -> modul tidak mengirim apa pun, ini masalah wiring");
  } else if (gps.failedChecksum() > gps.passedChecksum()) {
    Serial.println("  -> data masuk tapi banyak rusak, curigai baud atau kabel longgar");
  } else if (detik > 300) {
    Serial.println("  -> sudah lebih dari 5 menit, coba pindah ke luar ruangan");
  }

  Serial.println();
}
