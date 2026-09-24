#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#include "config.h"
#include "testingPacket.h"

// ============================================================
// COMMAND NODE versi UJI JALUR KOMUNIKASI.
//
// Tugasnya hanya dua:
//   1. mengirim teks yang diketik operator di Serial monitor
//   2. menampilkan paket yang datang dari field node
//
// Command node tidak punya GPS, jadi koordinat pada paket yang ia kirim
// selalu 0,0. Field tersebut tetap ada supaya format paket kedua node
// persis sama dan parsernya cuma satu.
// ============================================================

#define NODE_ID  ID_COMMAND

void terimaPaket();
void bacaInputOperator();
void kirimPesan(const String &teks);

String bufferInput;
unsigned long jumlahKirim = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("=== UJI COMMAND NODE (id %d) ===\n", NODE_ID);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // Turunkan clock SPI dari 8 MHz bawaan library. Di atas
  // breadboard dengan kabel dupont panjang, 8 MHz kerap menghasilkan
  // pembacaan register yang salah, dan register IRQ yang salah baca
  // itulah yang memunculkan "paket hantu" ribuan kali.
  LoRa.setSPIFrequency(2E6);

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

  Serial.println("LoRa: OK");
  Serial.printf("Batas panjang pesan %d karakter\n", PESAN_MAX_CHAR);
  Serial.println("Ketik pesan lalu tekan Enter untuk menyiarkan ke lapangan.");
  Serial.println();
}

void loop() {
  terimaPaket();
  bacaInputOperator();
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
    // Isinya dipotong 40 karakter pertama. Paket rusak sering berisi
    // ratusan byte sampah yang hanya membanjiri layar, sedangkan yang
    // berguna justru panjangnya: field node hanya mengirim sekitar 34
    // byte, jadi panjang 255 berarti pembacaan FIFO yang gagal, bukan
    // siaran node lain.
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
  if (p.lat == 0.0f && p.lon == 0.0f) {
    Serial.println("  perhatian: pengirim belum dapat fix GPS");
  }
  Serial.printf("  PESAN  \"%s\"\n", p.pesan.c_str());
  Serial.println();
}

void bacaInputOperator() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (bufferInput.length() > 0) {
        kirimPesan(bufferInput);
        bufferInput = "";
      }
      continue;
    }

    if (bufferInput.length() < PESAN_MAX_CHAR) {
      bufferInput += c;
    }
    // karakter berlebih dibuang, bukan disambung ke pesan berikutnya
  }
}

void kirimPesan(const String &teks) {
  // Koordinat 0,0: command node memang tidak punya GPS.
  String paket = buatPaketUji(NODE_ID, 0.0f, 0.0f, teks);

  // Penanda sebelum memancar. endPacket() memblokir sampai radio
  // melaporkan TxDone; kalau modul tidak pernah menjawab, program
  // menggantung di sana. Kalau baris MENGIRIM muncul tanpa disusul
  // TERKIRIM, itulah yang terjadi.
  Serial.printf("MENGIRIM: %s\n", paket.c_str());

  LoRa.beginPacket();
  LoRa.print(paket);
  LoRa.endPacket();

  jumlahKirim++;
  Serial.printf("TERKIRIM (%lu): %s\n\n", jumlahKirim, paket.c_str());
}
