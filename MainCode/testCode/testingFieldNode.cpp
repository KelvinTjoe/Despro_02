#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>

#include "config.h"
#include "testingPacket.h"

// ============================================================
// FIELD NODE versi UJI JALUR KOMUNIKASI.
//
// Tugasnya hanya dua:
//   1. menyiarkan paket berisi id + koordinat GPS + teks tiap 8 detik
//   2. menampilkan pesan yang datang dari command node
//
// Tidak ada tombol, tidak ada status personel, tidak ada relay, tidak
// ada anti-duplikasi. Semua itu ada di LoraFieldNode.cpp, bukan di sini.
// ============================================================

#define NODE_ID  ID_FIELD

// Jeda siaran periodik. Sengaja didefinisikan di file ini, bukan di
// config.h, supaya kode uji tidak mengubah perilaku node yang asli.
#define JEDA_KIRIM_MS  8000UL

// Teks yang dibawa tiap siaran periodik. Ubah sesuka hati saat pengujian.
#define PESAN_PERIODIK  "field node aktif"

// File .cpp tidak dapat prototipe otomatis seperti .ino,
// jadi setiap fungsi harus dideklarasikan sebelum dipakai.
void bacaGPS();
void kirimPeriodik();
void terimaPaket();
void laporGPS();

TinyGPSPlus gps;

// Posisi valid terakhir. Hanya diperbarui saat GPS benar-benar fix,
// sehingga nilainya tetap terpakai ketika sinyal satelit hilang.
float lastLat = 0.0f;
float lastLon = 0.0f;
unsigned long lastFixTime = 0;
bool pernahFix = false;

unsigned long jumlahKirim = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("=== UJI FIELD NODE (id %d) ===\n", NODE_ID);

  // Inisialisasi GPS NEO-6M di UART2, pin RX/TX sesuai config.h.
  // Cold start butuh minimal sekitar 30 detik sampai dapat fix.
  Serial2.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  Serial.println("GPS: UART2 siap, menunggu koordinat fix dari satelit");

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Turunkan clock SPI dari 8 MHz bawaan library ke 1 MHz. Di atas
  // breadboard dengan kabel dupont panjang, 8 MHz kerap menghasilkan
  // pembacaan register yang salah, dan register IRQ yang salah baca
  // itulah yang memunculkan "paket hantu" ribuan kali.
  LoRa.setSPIFrequency(1E6);

  // Kalau LoRa gagal init, berhenti di sini selamanya daripada
  // menyiarkan paket ke radio yang belum tentu menyala.
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("LoRa: init GAGAL, cek wiring SPI dan catu daya modul");
    while (1) {
      delay(1000);
    }
  }

  LoRa.setSpreadingFactor(LORA_SF);
  LoRa.setSignalBandwidth(LORA_BW);
  LoRa.setCodingRate4(LORA_CR);
  LoRa.setTxPower(LORA_TX_POWER);

  // CRC dinyalakan di kedua node. Radio membuang sendiri paket yang
  // rusak, jadi derau 433 MHz dan paket hantu tidak sampai ke kode.
  LoRa.enableCrc();

  Serial.printf("LoRa: OK pada %d MHz, SF%d\n", (int)(LORA_FREQ / 1E6), LORA_SF);
  Serial.printf("Menyiarkan paket tiap %lu detik\n", JEDA_KIRIM_MS / 1000);
  Serial.println("Menunggu pesan dari command node...");
  Serial.println();
}

void loop() {
  bacaGPS();
  kirimPeriodik();
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

// Siaran pertama dilakukan langsung saat boot, supaya tidak perlu
// menunggu satu periode penuh hanya untuk tahu radionya jalan.
void kirimPeriodik() {
  static unsigned long terakhirKirim = 0;
  static bool sudahPertama = false;
  unsigned long now = millis();

  if (sudahPertama && now - terakhirKirim < JEDA_KIRIM_MS) {
    return;
  }
  terakhirKirim = now;
  sudahPertama = true;

  // Koordinat 0,0 berarti GPS belum pernah fix sejak board menyala.
  float lat = pernahFix ? lastLat : 0.0f;
  float lon = pernahFix ? lastLon : 0.0f;

  String paket = buatPaketUji(NODE_ID, lat, lon, PESAN_PERIODIK);

  // Penanda sebelum memancar. endPacket() memblokir sampai radio
  // melaporkan TxDone; kalau baris MENGIRIM muncul tanpa disusul
  // TERKIRIM, berarti program menggantung menunggu modul menjawab.
  Serial.printf("MENGIRIM: %s\n", paket.c_str());

  LoRa.beginPacket();
  LoRa.print(paket);
  LoRa.endPacket();

  jumlahKirim++;
  Serial.printf("TERKIRIM (%lu): %s\n", jumlahKirim, paket.c_str());

  if (!pernahFix) {
    Serial.println("  peringatan: GPS belum pernah fix, koordinat dikirim 0,0");
  }
}

void terimaPaket() {
  int panjang = LoRa.parsePacket();
  if (panjang == 0) {
    return;
  }

  int rssi = LoRa.packetRssi();

  // Batasi pembacaan pada panjang yang dilaporkan radio, dan tidak
  // lebih dari BATAS_BACA. Jangan hanya mengandalkan LoRa.available(),
  // karena ia membaca ulang register panjang lewat SPI tiap putaran:
  // kalau modul menjawab ngawur, loop itu bisa tidak pernah berhenti
  // dan board tampak mati padahal sedang terjebak di sini.
  const int BATAS_BACA = 250;
  int sisa = panjang > BATAS_BACA ? BATAS_BACA : panjang;

  String raw;
  while (sisa > 0 && LoRa.available()) {
    raw += (char)LoRa.read();
    sisa--;
  }

  // Paket hantu: panjang dilaporkan > 0 tapi FIFO kosong, dan RSSI-nya
  // mustahil (-164 atau positif). Itu bukan siaran dari node lain,
  // melainkan pembacaan SPI yang kacau karena wiring atau catu daya
  // modul. Dihitung diam-diam dan dilaporkan sesekali, supaya tidak
  // membanjiri Serial dan menutupi paket yang asli.
  if (raw.length() == 0 || rssi > 0 || rssi < -150) {
    static unsigned long jumlahHantu = 0;
    static unsigned long terakhirLapor = 0;
    jumlahHantu++;
    if (millis() - terakhirLapor > 5000) {
      terakhirLapor = millis();
      Serial.printf("HANTU   : %lu paket tidak masuk akal diabaikan "
                    "(cek wiring SPI dan catu daya modul)\n", jumlahHantu);
    }
    return;
  }

  PaketUji p;
  if (!parsePaketUji(raw, p)) {
    // Panjang lebih berguna daripada isinya: paket sah dari command
    // node hanya puluhan byte, jadi ratusan byte sampah menandakan
    // pembacaan FIFO yang gagal, bukan siaran node lain.
    Serial.printf("RUSAK   : %d byte, RSSI %d, awalan \"%s\"\n",
                  raw.length(), rssi, raw.substring(0, 40).c_str());
    return;
  }

  if (!nodeDikenalUji(p.sourceID)) {
    Serial.printf("ASING   : sourceID %d tidak dikenal (RSSI %d)\n", p.sourceID, rssi);
    return;
  }

  // Paket siaran sendiri yang terdengar kembali, tidak perlu ditampilkan.
  if (p.sourceID == NODE_ID) {
    Serial.printf("GEMA    : siaran sendiri, diabaikan (RSSI %d)\n", rssi);
    return;
  }

  Serial.printf("DITERIMA: dari node %d, RSSI %d dBm\n", p.sourceID, rssi);
  Serial.printf("  POSISI %.6f, %.6f\n", p.lat, p.lon);
  Serial.printf("  PESAN  \"%s\"\n", p.pesan.c_str());
  Serial.println();
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
