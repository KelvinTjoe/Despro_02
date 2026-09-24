#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <esp_system.h>

#include "config.h"
#include "packet.h"

#define NODE_ID  ID_COMMAND

void terimaPaket();
void bacaInputOperator();
void kirimPesan(const String &teks);
void siarkan(const String &paket);

String bufferInput;

// Nomor urut paket berikutnya, lihat penjelasan di LoraFieldNode.cpp.
uint16_t seqBerikutnya = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.printf("=== COMMAND NODE (id %d) ===\n", NODE_ID);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

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

  seqBerikutnya = (uint16_t)(esp_random() & 0xFFFF);

  Serial.println("LoRa: OK");

#if HANYA_LANGSUNG
  Serial.println("MODE UJI: komunikasi langsung field <-> command saja.");
  Serial.println("          paket yang sudah diteruskan relay akan DITOLAK.");
  Serial.println("          ubah HANYA_LANGSUNG jadi 0 di config.h untuk normal.");
#else
  Serial.println("MODE NORMAL: paket langsung maupun via relay diterima.");
#endif
  Serial.printf("Tiap paket disiarkan %d kali (seq mulai %u)\n", ULANG_KIRIM, seqBerikutnya);

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

  String raw;
  while (LoRa.available()) {
    raw += (char)LoRa.read();
  }

  Paket p;
  if (!parsePaket(raw, p)) {
    Serial.printf("RUSAK  : \"%s\" (RSSI %d)\n", raw.c_str(), rssi);
    return;
  }

  HasilSaring hasil = saringPaket(p, NODE_ID);
  if (hasil != SARING_LOLOS) {
    Serial.printf("%-9s: dari node %d, lastHop %d, seq %u (RSSI %d)\n",
                  namaSaring(hasil), p.sourceID, p.lastHopID, p.seq, rssi);
    return;
  }

  Serial.printf("DITERIMA: dari node %d, seq %u, RSSI %d dBm\n",
                p.sourceID, p.seq, rssi);

  if (p.tipe == TIPE_STATUS) {
    Serial.printf("  STATUS %s\n", namaStatus(p.status));
    Serial.printf("  POSISI %.6f, %.6f (%s)\n",
                  p.lat, p.lon, namaKualitas(p.kualitas));
    if (p.kualitas == POS_NOFIX) {
      Serial.println("  perhatian: personel belum dapat fix GPS");
    } else if (p.kualitas == POS_STALE) {
      Serial.println("  perhatian: posisi sudah lama tidak diperbarui");
    }
  } else if (p.tipe == TIPE_PESAN) {
    Serial.printf("  PESAN  \"%s\"\n", p.teks.c_str());
  }

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
  // seq naik SETELAH dipakai, jadi siaran ulang membawa nomor yang sama.
  String paket = buatPaketPesan(NODE_ID, NODE_ID, seqBerikutnya, teks);
  seqBerikutnya++;

  siarkan(paket);
  Serial.println();
}

// Siarkan paket ULANG_KIRIM kali. delay() di sini boleh karena hanya
// terjadi saat operator menekan Enter, bukan di jalur penerimaan rutin.
void siarkan(const String &paket) {
  for (int i = 0; i < ULANG_KIRIM; i++) {
    if (i > 0) {
      delay(JEDA_ULANG_MS);
    }
    LoRa.beginPacket();
    LoRa.print(paket);
    LoRa.endPacket();
    Serial.printf("TERKIRIM (%d/%d): %s\n", i + 1, ULANG_KIRIM, paket.c_str());
  }
}
